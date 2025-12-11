#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <sys/select.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>   

#include "./shared/tcp.hpp"
#include "./shared/packet.hpp"
#include "./shared/protocol.hpp"
#include "./shared/json.hpp"
#include "./base64.hpp"
#include "../ThirdParty/minizip/miniunzip.hpp"


using nlohmann::json;

struct GameInfo {
    int         id = -1;
    std::string name;
    std::string desc;

    bool        installed = false;
    std::string installDir;

    std::string latestVersion;       
    std::string installedVersion;    
};

struct RoomInfo {
    std::string roomId;
    int         gameId = -1;
    std::vector<std::string> players;
    bool        amHost = false;
};


enum class ClientLaunchMode {
    None,
    GUI,
    CLI
};


static const std::string DOWNLOAD_ROOT = "downloads";

static std::string makePlayerRoot(int playerId) {
    return DOWNLOAD_ROOT + "/player_" + std::to_string(playerId);
}

static std::string makeGameInstallDir(int playerId, int gameId) {
    return makePlayerRoot(playerId) + "/game_" + std::to_string(gameId);
}

static bool ensureDir(const std::string &path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[GUI] Failed to create directory " << path
                  << ": " << e.what() << "\n";
        return false;
    }
}

static bool writeBinaryFile(const std::string &path,
                            const std::vector<uint8_t> &data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[GUI] Cannot open " << path << " for writing.\n";
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return true;
}

bool checkInstalled(int playerId, int gameId, std::string &outDir) {
    std::string dir = makeGameInstallDir(playerId, gameId);
    if (std::filesystem::exists(dir)) {
        outDir = dir;
        return true;
    }
    return false;
}

bool unzipFileWithSystem(const std::string &zipPath, const std::string &outputDir)
{
    std::string cmd = "unzip -o \"" + zipPath + "\" -d \"" + outputDir + "\"";
    int code = std::system(cmd.c_str());
    return (code == 0);
}

static void markExecutablesInDir(const std::string &dirPath) {
    namespace fs = std::filesystem;
    if (!fs::exists(dirPath)) return;
    for (auto &entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        const std::string p = entry.path().string();
        ::chmod(p.c_str(), 0755);
    }
}
static std::string readVersionFile(const std::string &installDir) {
    std::string path = installDir + "/version.txt";
    if (!std::filesystem::exists(path)) return "";

    std::ifstream f(path);
    std::string ver;
    std::getline(f, ver);
    return ver;
}
class GameStoreApp {
public:
    GameStoreApp(const std::string &host, int port)
        : m_host(host),
          m_port(port),
          m_window(sf::VideoMode(900, 700), "Game Store & Lobby") {

        if (!m_font.loadFromFile("assets/FreeSans.ttf")) {
            std::cerr << "[GUI] Fatal: could not load font.\n";
            exit(1);
        }

        if (!m_conn.connectToServer(host, port)) {
            std::cerr << "[GUI] Could not connect to main store server at "
                      << host << ":" << port << "\n";
            m_running = false;
            return;
        }

        std::cout << "[GUI] Connected to store server " << host << ":" << port << "\n";

        m_view = View::Login;
        m_running = true;
        m_selectedGameIndex = 0;

        m_statusMessage   = "Please login or register.";
        m_statusIsError   = false;
        m_loginSuccessTimer = 0.0f;
    }

    void run() {
        sf::Clock clock;
        while (m_window.isOpen() && m_running) {
            float dt = clock.restart().asSeconds();

            pollNetwork();

            sf::Event ev;
            while (m_window.pollEvent(ev)) {
                if (ev.type == sf::Event::Closed) {
                    m_window.close();
                    m_running = false;
                } else {
                    handleEvent(ev);
                }
            }

            update(dt);
            draw();
        }
    }

private:

    enum class View {
        Login,
        Store,
        Room
    };

    enum class LoginField {
        None,
        Username,
        Password
    };

    void sendPlayerLogin() {
        if (m_loginUser.empty() || m_loginPass.empty()) {
            m_statusMessage = "Username and password required.";
            m_statusIsError = true;
            return;
        }

        Packet p;
        p.type = PacketType::PLAYER_LOGIN;
        p.data["username"] = m_loginUser;
        p.data["password"] = m_loginPass;
        m_conn.sendPacket(p);

        m_statusMessage = "Logging in...";
        m_statusIsError = false;
        std::cout << "[GUI] Sent PLAYER_LOGIN for username=" << m_loginUser << "\n";
    }

    void sendPlayerRegister() {
        if (m_loginUser.empty() || m_loginPass.empty()) {
            m_statusMessage = "Username and password required.";
            m_statusIsError = true;
            return;
        }

        Packet p;
        p.type = PacketType::PLAYER_REGISTER;
        p.data["username"] = m_loginUser;
        p.data["password"] = m_loginPass;
        m_conn.sendPacket(p);

        m_statusMessage = "Registering account...";
        m_statusIsError = false;
        std::cout << "[GUI] Sent PLAYER_REGISTER for username=" << m_loginUser << "\n";
    }

    std::string parseRoomId(const json &d) {
        if (!d.contains("room_id")) return "???";

        if (d["room_id"].is_string())
            return d["room_id"].get<std::string>();

        if (d["room_id"].is_number())
            return std::to_string(d["room_id"].get<int>());

        return "???";
    }

    void handlePlayerLogin(const nlohmann::json &d) {
        bool ok = d.value("ok", false);
        ensureDir(makePlayerRoot(m_playerId));
        if (!ok) {
            m_statusMessage = d.value("msg", "Login failed.");
            m_statusIsError = true;
            return;             
        }

        int pid = d.value("player_id", -1);
        if (pid <= 0) {
            m_statusMessage = "Login failed.";
            m_statusIsError = true;
            return;
        }

        m_playerId = pid;
        m_statusMessage = "Login success! Entering store...";
        m_statusIsError = false;

        requestGameList();
        m_loginSuccessTimer = 0.8f;
    }

    void pollNetwork() {
        int fd = m_conn.fd();
        if (fd < 0) return;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0; 

        int ret = select(fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            perror("select");
            m_running = false;
            return;
        }

        if (ret == 0) {
            return;
        }

        if (FD_ISSET(fd, &readfds)) {
            Packet p;
            if (!m_conn.recvPacket(p)) {
                std::cerr << "[GUI] Disconnected from store server\n";
                m_running = false;
                return;
            }
            handlePacket(p);
        }
    }

    void handlePacket(const Packet &p) {
        const auto &d = p.data;
        //std::cout<<d.value("kind",":)")<<"\n";
        // First, detect login/register success response
        if (d.contains("player_id") && d.value("ok", true)) {
            handlePlayerLogin(d);
            return;
        }
        if (d.contains("reviews")) {
            m_currentReviews.clear();
            for (auto &r : d["reviews"])
                m_currentReviews.push_back(r);

            m_statusMessage = "Loaded reviews.";
            m_statusIsError = false;
            return;
        }
        if (p.type == PacketType::ERROR_RESPONSE) {
            std::string msg = d.value("msg",
                                d.value("message",
                                        std::string("Unknown error")));
            m_statusMessage = "Server error: " + msg;
            m_statusIsError = true;
            std::cerr << "[GUI] ERROR_RESPONSE: " << msg << "\n";
            return;
        }

        if (p.type != PacketType::SERVER_RESPONSE) {
            return;
        }

        bool ok = d.value("ok", true);
        if (!ok) {
            std::string msg = d.value("msg",
                              d.value("message",
                                      std::string("Request failed")));
            m_statusMessage = msg;
            m_statusIsError = true;
            std::cerr << "[GUI] SERVER_RESPONSE error: " << msg << "\n";
            return;
        }
        if (d.value("kind", ":(")=="START_GAME") std::cout<<"startgamepacketreceived\n";

        if (d.contains("games")) {
            handleGameList(d);
        } else if (d.contains("filedata_base64")) {
            handleGameDownload(d);
        } else if (d.contains("server_port") && d.contains("game_id")) {
            std::cout<<"startinggamereponse\n";
            handleStartGameResponse(d);
        } else if (d.contains("room_id")) {
            handleRoomInfo(d);
        } else {
            std::cout<<"unknown response\n";
        }
    }

    //GAME LIST
    void handleGameList(const nlohmann::json &d) {
        if (!d.contains("games")) return;

        m_games.clear();

        for (auto &g : d["games"]) {
            GameInfo gi;
            gi.id   = g.value("game_id", -1);
            gi.name = g.value("name", std::string("Unknown Game"));
            gi.desc = g.value("description",
                       g.value("desc", std::string("")));
            gi.latestVersion = g.value("latest_version", std::string("0.0.0"));
            std::string dir;
            gi.installed = (gi.id > 0) && checkInstalled(m_playerId, gi.id, dir);
            gi.installDir = dir;
            if (gi.installed)
                gi.installedVersion = readVersionFile(gi.installDir);
            else
                gi.installedVersion = "";
            m_games.push_back(gi);
        }

        m_selectedGameIndex = 0;
        m_statusMessage = "Received " + std::to_string(m_games.size()) + " game(s) from server.";
        m_statusIsError = false;
        std::cout << "[GUI] Received " << m_games.size() << " game(s) from server\n";
    }

    //ROOM INFO 
    void handleRoomInfo(const nlohmann::json &d) {
        RoomInfo info;
        info.roomId = parseRoomId(d);
        info.gameId = d.value("game_id", -1);
        info.players.clear();
        info.amHost = m_lastRoomHostFlag;  

        if (d.contains("players")) {
            for (auto &p : d["players"]) {
                if (p.is_string())
                    info.players.push_back(p.get<std::string>());
                else if (p.is_number())
                    info.players.push_back("Player " + std::to_string(p.get<int>()));
            }
        }

        m_room = info;
        m_view = View::Room;

        m_statusMessage = "Joined room " + info.roomId;
        m_statusIsError = false;
        std::cout << "[GUI] Room info: room_id=" << m_room->roomId
                  << " players=" << m_room->players.size()
                  << " amHost=" << (m_room->amHost ? 1 : 0) << "\n";
    }

    //GAME DOWNLOAD
    void handleGameDownload(const nlohmann::json &d) {
        if (m_selectedGameIndex < 0 ||
            m_selectedGameIndex >= static_cast<int>(m_games.size())) {
            m_statusMessage = "Download result but no selected game?";
            m_statusIsError = true;
            return;
        }

        auto &game = m_games[m_selectedGameIndex];

        std::string filename = d.value("filename", std::string("game.zip"));
        std::string b64      = d.value("filedata_base64", std::string(""));
        std::string version  = d.value("version", std::string(""));

        if (b64.empty()) {
            m_statusMessage = "Download failed: empty filedata.";
            m_statusIsError = true;
            return;
        }

        m_statusMessage = "Decoding game package...";
        m_statusIsError = false;
        std::vector<uint8_t> raw;
        try {
            raw = decodeBase64(b64);
        } catch (const std::exception &e) {
            std::cerr << "[GUI] Base64 decode error: " << e.what() << "\n";
            m_statusMessage = "Base64 decode failed.";
            m_statusIsError = true;
            return;
        }

        std::string installDir = makeGameInstallDir(m_playerId, game.id);
        if (!ensureDir(installDir)) {
            m_statusMessage = "Cannot create install directory.";
            m_statusIsError = true;
            return;
        }

        std::string zipPath = installDir + "/" + filename;
        if (!writeBinaryFile(zipPath, raw)) {
            m_statusMessage = "Failed to write game zip.";
            m_statusIsError = true;
            return;
        }

        m_statusMessage = "Unzipping game package...";
        m_statusIsError = false;

        std::string cmd =
            "unzip -o \"" + zipPath + "\" -d \"" + installDir + "\" > /dev/null 2>&1";

        int code = std::system(cmd.c_str());
        if (code != 0) {
            m_statusMessage = "Failed to unzip game package.";
            m_statusIsError = true;
            std::cerr << "[GUI] unzip command failed: " << cmd << "\n";
            return;
        }

        markExecutablesInDir(installDir + "/server");
        markExecutablesInDir(installDir + "/client");
        std::string vpath = installDir + "/version.txt";
        std::ofstream vf(vpath);
        vf << version;
        vf.close();

        game.installedVersion = version;
        game.installed = true;
        game.installDir = installDir;
        m_isDownloading = false;

        m_statusMessage = "Installed '" + game.name + "' (version " + version +
                          ") to " + installDir;
        m_statusIsError = false;

        std::cout << "[GUI] Installed game " << game.name
                  << " into " << installDir << "\n";
    }

    //START GAME response
    void handleStartGameResponse(const nlohmann::json &d) {
        int gameId = d.value("game_id", -1);
        int port   = d.value("server_port", 0);
        std::string is_host = d.value("is_host","0");
        std::cout<<"starting game on port: "<<port<<"\n";
        if (gameId < 0 || port <= 0) {
            m_statusMessage = "Invalid start-game response.";
            m_statusIsError = true;
            return;
        }

        GameInfo *g = nullptr;
        for (auto &gi : m_games) {
            if (gi.id == gameId) {
                g = &gi;
                break;
            }
        }
        if (!g || !g->installed) {
            m_statusMessage = "Game not installed locally; cannot launch client.";
            m_statusIsError = true;
            return;
        }

        if (m_pendingLaunchMode == ClientLaunchMode::None) {
            m_pendingLaunchMode = ClientLaunchMode::GUI;
        }

        std::string exeRel;
        if (m_pendingLaunchMode == ClientLaunchMode::GUI) {
            exeRel = g->installDir + "/client/run_gui.sh";
        } else {
            exeRel = g->installDir + "/client/run_cli.sh";
        }

        std::string exeAbs = std::filesystem::absolute(exeRel).string();

        //executable permissions
        ::chmod(exeAbs.c_str(), 0755);
        /*
        std::string cmd =
            "/bin/bash -lc \"" + exeAbs + " " + m_host + " " +
            std::to_string(port) + " " +
            std::to_string(myPlayerId) +" " +
            is_host +" & disown\"";

        std::cout << "[GUI] Launching: " << cmd << "\n";

        m_statusMessage = "Launching client on port " + std::to_string(port) + "...";
        m_statusIsError = false;
        std::cout << "start game command\n";
        std::system(cmd.c_str());
        std::cout << "start game command complete\n";
        */
        pid_t pid = fork();
        if (pid == 0) {
            
            execl(exeAbs.c_str(),
                exeAbs.c_str(),
                m_host.c_str(),
                std::to_string(port).c_str(),
                is_host.c_str(),
                (char*)NULL);

            _exit(1);
        }
        m_pendingLaunchMode = ClientLaunchMode::None;
    
    }

    //Outgoing requests
    void requestGameList() {
        Packet p;
        p.type = PacketType::PLAYER_LIST_GAMES;
        m_conn.sendPacket(p);
        m_statusMessage = "Requested game list from server...";
        m_statusIsError = false;
    }

    void downloadSelectedGame() {
        if (m_selectedGameIndex < 0 ||
            m_selectedGameIndex >= static_cast<int>(m_games.size())) {
            m_statusMessage = "No game selected.";
            m_statusIsError = true;
            return;
        }
        auto &g = m_games[m_selectedGameIndex];
        if (g.installed && g.installedVersion == g.latestVersion) {
            m_statusMessage = "Already latest version.";
            m_statusIsError = true;
            return;
        }

        Packet p;
        p.type = PacketType::PLAYER_DOWNLOAD_GAME;
        p.data["game_id"] = g.id;
        m_conn.sendPacket(p);

        m_isDownloading = true;
        m_statusMessage = "Downloading '" + g.name + "' from server...";
        m_statusIsError = false;
        std::cout << "[GUI] Sent PLAYER_DOWNLOAD_GAME for game_id=" << g.id << "\n";
    }

    void createRoomForSelectedGame() {
        if (m_selectedGameIndex < 0 ||
            m_selectedGameIndex >= static_cast<int>(m_games.size())) {
            m_statusMessage = "No game selected.";
            m_statusIsError = true;
            return;
        }
        auto &g = m_games[m_selectedGameIndex];
        if (!g.installed) {
            m_statusMessage = "Please download/install the game before creating a room.";
            m_statusIsError = true;
            return;
        }
        if (g.installedVersion != g.latestVersion) {
            m_statusMessage = "Update required: installed " + g.installedVersion +
                            ", latest " + g.latestVersion;
            m_statusIsError = true;
            return;
        }
        Packet p;
        p.type = PacketType::PLAYER_CREATE_ROOM;
        p.data["game_id"]   = g.id;
        p.data["player_id"] = m_playerId;
        m_conn.sendPacket(p);

        m_lastRoomHostFlag = true;
        m_statusMessage = "Creating room for '" + g.name + "'...";
        m_statusIsError = false;
        std::cout << "[GUI] Sent PLAYER_CREATE_ROOM for game_id=" << g.id << "\n";
    }

    void joinRoomByCode(const std::string &roomCode) {
        std::cout<<"joinRoomByCode:  "<< roomCode<<"\n"; 
        if (roomCode == "ROOM CODE") return;
        if (m_selectedGameIndex < 0 ||
            m_selectedGameIndex >= static_cast<int>(m_games.size())) {
            m_statusMessage = "No game selected.";
            m_statusIsError = true;
            return;
        }
        auto &g = m_games[m_selectedGameIndex];
        if (!g.installed) {
            m_statusMessage = "Please download/install the game before joining a room.";
            m_statusIsError = true;
            return;
        }
        if (g.installedVersion != g.latestVersion) {
            m_statusMessage = "You must update this game before joining rooms!";
            m_statusIsError = true;
            return;
        }

        Packet p;
        p.type = PacketType::PLAYER_JOIN_ROOM;
        try {
            p.data["room_id"] = std::stoi(roomCode);
        } catch (...) {
            m_statusMessage = "Invalid room code!";
            m_statusIsError = true;
            return;
        }
        p.data["player_id"] = m_playerId;
        m_conn.sendPacket(p);

        m_lastRoomHostFlag = false;
        m_statusMessage = "Joining room " + roomCode + "...";
        m_statusIsError = false;
        std::cout << "[GUI] Sent PLAYER_JOIN_ROOM room_id=" << roomCode << "\n";
    }

    void sendStartGame() {
        if (!m_room) {
            m_statusMessage = "No active room.";
            m_statusIsError = true;
            return;
        }
        if (m_room->roomId.empty() ||
            !std::all_of(m_room->roomId.begin(), m_room->roomId.end(), ::isdigit)) {
            m_statusMessage = "Invalid room ID.";
            m_statusIsError = true;
            return;
        }
        Packet p;
        p.type = PacketType::PLAYER_START_GAME;
        // server expects integer room_id
        //std::cout<<"sendStartGame stoi\n";
        p.data["room_id"]   = std::stoi(m_room->roomId);
        //std::cout<<"sendStartGame stoi fin\n";
        p.data["player_id"] = m_playerId;

        m_conn.sendPacket(p);

        m_statusMessage = "Requested game start for room " + m_room->roomId + "...";
        m_statusIsError = false;
        std::cout << "[GUI] Sent PLAYER_START_GAME for room " << m_room->roomId << "\n";
    }
    void sendSubmitReview() {
        if (m_selectedGameIndex < 0 || m_selectedGameIndex >= (int)m_games.size()) {
            m_statusMessage = "No game selected.";
            m_statusIsError = true;
            return;
        }

        if (m_reviewScore < 1 || m_reviewScore > 5) {
            m_statusMessage = "Score must be 1–5.";
            m_statusIsError = true;
            return;
        }

        Packet p;
        p.type = PacketType::PLAYER_SUBMIT_REVIEW;
        p.data["player_id"] = m_playerId;
        p.data["game_id"]   = m_games[m_selectedGameIndex].id;
        p.data["score"]     = m_reviewScore;
        p.data["comment"]  = m_reviewText;

        m_conn.sendPacket(p);

        m_statusMessage = "Submitting review...";
        m_statusIsError = false;

        m_reviewBoxOpen = false;

        Packet q;
        q.type = PacketType::PLAYER_GET_REVIEWS;
        q.data["game_id"] = m_games[m_selectedGameIndex].id;
        m_conn.sendPacket(q);

    }

    //SFML
    void handleEvent(const sf::Event &ev) {
        if (m_view == View::Login) {
            handleLoginEvent(ev);
        } else if (m_view == View::Store) {
            handleStoreEvent(ev);
        } else if (m_view == View::Room) {
            handleRoomEvent(ev);
        }
    }

    void handleLoginEvent(const sf::Event &ev) {
        if (ev.type == sf::Event::MouseButtonPressed &&
            ev.mouseButton.button == sf::Mouse::Left) {

            sf::Vector2f mouse(ev.mouseButton.x, ev.mouseButton.y);

            if (m_loginUserBox.getGlobalBounds().contains(mouse)) {
                m_loginActiveField = LoginField::Username;
            } else if (m_loginPassBox.getGlobalBounds().contains(mouse)) {
                m_loginActiveField = LoginField::Password;
            } else if (m_loginButton.getGlobalBounds().contains(mouse)) {
                sendPlayerLogin();
            } else if (m_registerButton.getGlobalBounds().contains(mouse)) {
                sendPlayerRegister();
            } else {
                m_loginActiveField = LoginField::None;
            }
        } else if (ev.type == sf::Event::TextEntered) {
            if (ev.text.unicode == '\b') {
                std::string *field = currentLoginField();
                if (field && !field->empty()) field->pop_back();
            } else if (ev.text.unicode == '\r' || ev.text.unicode == '\n') {
                sendPlayerLogin();
            } else if (ev.text.unicode >= 32 && ev.text.unicode < 127) {
                std::string *field = currentLoginField();
                if (field) {
                    field->push_back(static_cast<char>(ev.text.unicode));
                }
            }
        }
    }

    std::string* currentLoginField() {
        if (m_loginActiveField == LoginField::Username) return &m_loginUser;
        if (m_loginActiveField == LoginField::Password) return &m_loginPass;
        return nullptr;
    }

    void handleStoreEvent(const sf::Event &ev) {
        //Scroll wheel 
        if (m_reviewBoxOpen) {
            if (ev.type == sf::Event::MouseButtonPressed) {
                sf::Vector2f m(ev.mouseButton.x, ev.mouseButton.y);

                if (m_reviewSubmitBtn.getGlobalBounds().contains(m)) {
                    sendSubmitReview();
                    return;
                }

                if (m_reviewCancelBtn.getGlobalBounds().contains(m)) {
                    m_reviewBoxOpen = false;
                    return;
                }
            }

            if (ev.type == sf::Event::KeyPressed) {
                if (ev.key.code == sf::Keyboard::Up && m_reviewScore < 5)
                    m_reviewScore++;
                if (ev.key.code == sf::Keyboard::Down && m_reviewScore > 1)
                    m_reviewScore--;
            }

            if (ev.type == sf::Event::TextEntered) {
                if (ev.text.unicode == '\b' && !m_reviewText.empty())
                    m_reviewText.pop_back();
                else if (ev.text.unicode >= 32 && ev.text.unicode < 127)
                    m_reviewText.push_back((char)ev.text.unicode);
            }

            return;
        }
        if (ev.type == sf::Event::MouseWheelScrolled) {
            m_scrollOffset -= ev.mouseWheelScroll.delta * 80.f; // scroll speed

            if (m_scrollOffset < 0) m_scrollOffset = 0;

            int maxOffset = std::max(0,
                (int)m_games.size() * 120 - 350); // 120 = card spacing, 350 = window space

            if (m_scrollOffset > maxOffset)
                m_scrollOffset = maxOffset;

            return;
        }

        //Click to select a game
        if (ev.type == sf::Event::MouseButtonPressed &&
            ev.mouseButton.button == sf::Mouse::Left)
        {
            sf::Vector2f mouse(ev.mouseButton.x, ev.mouseButton.y);

            float listX = 60.f;
            float startY = 100.f - m_scrollOffset;

            for (int i = 0; i < (int)m_games.size(); i++) {
                float cardY = startY + i * 120.f;
                sf::FloatRect cardArea(listX, cardY, 780.f, 100.f);

                if (cardArea.contains(mouse)) {
                    m_selectedGameIndex = i;

                    Packet p;
                    p.type = PacketType::PLAYER_GET_REVIEWS;
                    p.data["game_id"] = m_games[i].id;
                    m_conn.sendPacket(p);

                    return;
                }
            }

            //Buttons
            if (m_downloadButton.getGlobalBounds().contains(mouse)) {
                downloadSelectedGame();
            } else if (m_createButton.getGlobalBounds().contains(mouse)) {
                createRoomForSelectedGame();
            } else if (m_joinButton.getGlobalBounds().contains(mouse)) {
                    if (!m_joinCode.empty()) {
                        joinRoomByCode(m_joinCode);
                        m_joinInputActive = false;
                    } else {
                        m_joinInputActive = true;
                        m_statusMessage = "Enter room code...";
                        m_statusIsError = false;
                    }
            }else {
                m_joinInputActive = false;
            }

            sf::FloatRect joinBox(700, 505, 200, 40);  
            if (joinBox.contains(mouse)) {
                m_joinInputActive = true;
                return;
            }
            if (m_reviewOpenBtn.getGlobalBounds().contains(mouse)) {
                if (m_games.empty()) {
                    m_statusMessage = "No game selected.";
                    m_statusIsError = true;
                    return;
                }

                m_reviewBoxOpen = true;
                m_reviewScore = 5;
                m_reviewText.clear();
                return;
            }
            if (m_refreshButton.getGlobalBounds().contains(mouse)) {
                requestGameList();
                m_statusMessage = "Refreshing game list from server...";
                m_statusIsError = false;
                return;
            }
        }
        if (m_reviewBoxOpen && ev.type == sf::Event::MouseButtonPressed) {
            sf::Vector2f m(ev.mouseButton.x, ev.mouseButton.y);

            if (m_reviewSubmitBtn.getGlobalBounds().contains(m)) {
                sendSubmitReview();
                return;
            }

            if (m_reviewCancelBtn.getGlobalBounds().contains(m)) {
                m_reviewBoxOpen = false;
                return;
            }
        }

        if (m_reviewBoxOpen && ev.type == sf::Event::KeyPressed) {
            if (ev.key.code == sf::Keyboard::Up && m_reviewScore < 5)
                m_reviewScore++;
            if (ev.key.code == sf::Keyboard::Down && m_reviewScore > 1)
                m_reviewScore--;
        }

        if (m_reviewBoxOpen) {
            if (ev.type == sf::Event::TextEntered) {
                if (ev.text.unicode == '\b' && !m_reviewText.empty())
                    m_reviewText.pop_back();
                else if (ev.text.unicode >= 32 && ev.text.unicode < 127)
                    m_reviewText.push_back((char)ev.text.unicode);
            }
            return; 
        }


        //Enter room code
        if (ev.type == sf::Event::TextEntered && m_joinInputActive) {
            if (ev.text.unicode == '\b') {
                if (!m_joinCode.empty()) m_joinCode.pop_back();
            } else if (ev.text.unicode == '\r' || ev.text.unicode == '\n') {
                if (!m_joinCode.empty()) {
                    joinRoomByCode(m_joinCode);
                    m_joinInputActive = false;
                }
            } else if (ev.text.unicode < 128) {
                char c = static_cast<char>(ev.text.unicode);
                if (!std::isspace((unsigned char)c) &&
                    m_joinCode.size() < 12)
                {
                    m_joinCode.push_back(c);
                }
            }
        }
    }

    void handleRoomEvent(const sf::Event &ev) {
        if (!m_room) return;

        if (ev.type == sf::Event::KeyPressed) {
            if (ev.key.code == sf::Keyboard::Escape) {
                m_view = View::Store;
                m_room.reset();
                m_statusMessage = "Returned to store.";
                m_statusIsError = false;
            }
        }

        if (ev.type == sf::Event::MouseButtonPressed &&
            ev.mouseButton.button == sf::Mouse::Left) {

            sf::Vector2f mouse(ev.mouseButton.x, ev.mouseButton.y);

            if (m_room->amHost) {
                if (m_roomStartGuiButton.getGlobalBounds().contains(mouse)) {
                    m_pendingLaunchMode = ClientLaunchMode::GUI;
                    sendStartGame();
                } else if (m_roomStartCliButton.getGlobalBounds().contains(mouse)) {
                    m_pendingLaunchMode = ClientLaunchMode::CLI;
                    sendStartGame();
                }
            }
        }
    }

    void update(float dt) {
        if (m_view == View::Login && m_loginSuccessTimer > 0.0f) {
            m_loginSuccessTimer -= dt;
            if (m_loginSuccessTimer <= 0.0f && m_playerId > 0) {
                m_view = View::Store;
            }
        }
    }

    void draw() {
        m_window.clear(sf::Color(30, 30, 30));

        if (m_view == View::Login) {
            //std::cout<<"drawLoginView\n";
            drawLoginView();
        } else if (m_view == View::Store) {
            //std::cout<<"drawStoreView\n";
            drawStoreView();
        } else if (m_view == View::Room) {
            //std::cout<<"drawRoomView\n";
            drawRoomView();
        }
        if (m_reviewBoxOpen)
            drawReviewPopup();

        m_window.display();
    }


    void drawLoginView() {
        sf::Text title;
        title.setFont(m_font);
        title.setCharacterSize(36);
        title.setFillColor(sf::Color::White);
        title.setString("Player Login / Register");
        title.setPosition(60, 60);
        m_window.draw(title);

        float x = 60.f;
        float y = 150.f;
        float w = 360.f;
        float h = 40.f;

        sf::Text labelUser;
        labelUser.setFont(m_font);
        labelUser.setCharacterSize(18);
        labelUser.setFillColor(sf::Color(220, 220, 220));
        labelUser.setString("Username");
        labelUser.setPosition(x, y - 26);
        m_window.draw(labelUser);

        m_loginUserBox.setSize(sf::Vector2f(w, h));
        m_loginUserBox.setPosition(x, y);
        m_loginUserBox.setFillColor(
            (m_loginActiveField == LoginField::Username)
            ? sf::Color(255, 255, 255)
            : sf::Color(230, 230, 230)
        );
        m_loginUserBox.setOutlineColor(sf::Color::Black);
        m_loginUserBox.setOutlineThickness(2);
        m_window.draw(m_loginUserBox);

        sf::Text userText;
        userText.setFont(m_font);
        userText.setCharacterSize(20);
        userText.setFillColor(sf::Color::Black);
        userText.setString(m_loginUser);
        userText.setPosition(x + 8, y + 6);
        m_window.draw(userText);

        y += 80.f;

        sf::Text labelPass;
        labelPass.setFont(m_font);
        labelPass.setCharacterSize(18);
        labelPass.setFillColor(sf::Color(220, 220, 220));
        labelPass.setString("Password");
        labelPass.setPosition(x, y - 26);
        m_window.draw(labelPass);

        m_loginPassBox.setSize(sf::Vector2f(w, h));
        m_loginPassBox.setPosition(x, y);
        m_loginPassBox.setFillColor(
            (m_loginActiveField == LoginField::Password)
            ? sf::Color(255, 255, 255)
            : sf::Color(230, 230, 230)
        );
        m_loginPassBox.setOutlineColor(sf::Color::Black);
        m_loginPassBox.setOutlineThickness(2);
        m_window.draw(m_loginPassBox);

        sf::Text passText;
        passText.setFont(m_font);
        passText.setCharacterSize(20);
        passText.setFillColor(sf::Color::Black);
        passText.setString(std::string(m_loginPass.size(), '*'));
        passText.setPosition(x + 8, y + 6);
        m_window.draw(passText);

        // Buttons
        float btnY = y + 80.f;

        m_loginButton.setSize(sf::Vector2f(160.f, 46.f));
        m_loginButton.setPosition(x, btnY);
        m_loginButton.setFillColor(sf::Color(120, 180, 120));
        m_window.draw(m_loginButton);

        sf::Text loginText;
        loginText.setFont(m_font);
        loginText.setCharacterSize(22);
        loginText.setFillColor(sf::Color::Black);
        loginText.setString("Login");
        loginText.setPosition(x + 40, btnY + 8);
        m_window.draw(loginText);

        m_registerButton.setSize(sf::Vector2f(160.f, 46.f));
        m_registerButton.setPosition(x + 190, btnY);
        m_registerButton.setFillColor(sf::Color(120, 120, 200));
        m_window.draw(m_registerButton);

        sf::Text regText;
        regText.setFont(m_font);
        regText.setCharacterSize(22);
        regText.setFillColor(sf::Color::Black);
        regText.setString("Register");
        regText.setPosition(x + 220, btnY + 8);
        m_window.draw(regText);

        //Small hint text
        sf::Text hint;
        hint.setFont(m_font);
        hint.setCharacterSize(14);
        hint.setFillColor(sf::Color(200, 200, 200));
        hint.setString(
            "If you register with an existing username,\n"
            "the server will return an error.\n"
            "If you login with an already logged-in account,\n"
            "the server will also return an error."
        );
        hint.setPosition(60, btnY + 70);
        m_window.draw(hint);

        drawStatusLine();
    }


    void drawStoreView() {
        sf::Text title;
        title.setFont(m_font);
        title.setCharacterSize(36);
        title.setFillColor(sf::Color::White);
        title.setString("Game Store");
        title.setPosition(40, 20);
        m_window.draw(title);

        //game cards
        float listX = 60.f;
        float startY = 100.f - m_scrollOffset;

        for (int i = 0; i < (int)m_games.size(); i++) {
            GameInfo &g = m_games[i];

            float cardY = startY + i * 120.f;

            sf::RectangleShape card(sf::Vector2f(780.f, 100.f));
            card.setPosition(listX, cardY);
            card.setFillColor(sf::Color(50, 50, 80));

            // highlight selected game
            card.setOutlineColor(
                (i == m_selectedGameIndex)
                ? sf::Color(180, 180, 80)
                : sf::Color(120, 120, 200)
            );
            card.setOutlineThickness(2);
            m_window.draw(card);

            // Game name
            sf::Text name(g.name + " (ID " + std::to_string(g.id) + ")", m_font, 24);
            name.setFillColor(sf::Color::White);
            name.setPosition(listX + 20, cardY + 8);
            m_window.draw(name);

            // Description
            sf::Text desc(g.desc, m_font, 16);
            desc.setFillColor(sf::Color(220, 220, 220));
            desc.setPosition(listX + 20, cardY + 45);
            m_window.draw(desc);

            sf::Text ver("Installed: " +
                        (g.installed ? g.installedVersion : std::string("none")) +
                        " / Latest: " + g.latestVersion, m_font, 16);

            ver.setFillColor(sf::Color(200,200,255));
            ver.setPosition(listX + 20, cardY + 65);
            m_window.draw(ver);

            // Install state
            sf::Text install;
            install.setFont(m_font);
            install.setCharacterSize(16);
            install.setFillColor(sf::Color(200, 255, 200));
            install.setPosition(listX + 20, cardY + 82);
            install.setString(g.installed ? "Installed" : "Not installed");
            m_window.draw(install);
            if (g.installed && g.installedVersion != g.latestVersion) {
                sf::Text up("Update Available!", m_font, 16);
                up.setFillColor(sf::Color(255,180,80));
                up.setPosition(listX + 600, cardY + 10);
                m_window.draw(up);
            }

        }
        float ry = 580;
        sf::Text rvTitle("Reviews:", m_font, 20);
        rvTitle.setFillColor(sf::Color::White);
        rvTitle.setPosition(40, ry);
        m_window.draw(rvTitle);

        ry += 26;

        for (auto &r : m_currentReviews) {
            std::string line =
                "Score: " + std::to_string(r.value("score", 0)) +
                " - " + r.value("comment", std::string(""));

            sf::Text txt(line, m_font, 16);
            txt.setFillColor(sf::Color(220,220,220));
            txt.setPosition(40, ry);
            m_window.draw(txt);

            ry += 22;
        }

        if (!m_games.empty()) {
            const GameInfo &g = m_games[m_selectedGameIndex];

            float buttonBaseY = 500.f;

            // Download button
            m_downloadButton.setSize(sf::Vector2f(160, 50));
            m_downloadButton.setPosition(60, buttonBaseY);

            if (!g.installed)
                m_downloadButton.setFillColor(sf::Color(200, 200, 80));   // Download
            else if (g.installedVersion != g.latestVersion)
                m_downloadButton.setFillColor(sf::Color(200, 160, 80));  // Update
            else
                m_downloadButton.setFillColor(sf::Color(100, 100, 100)); // Latest

            m_window.draw(m_downloadButton);

            sf::Text dlText(
                (g.installed && g.installedVersion != g.latestVersion)
                    ? "Update"
                    : (g.installed ? "Installed" : "Download"),
                m_font, 20
            );
            if (m_isDownloading) dlText.setString("Downloading...");
            dlText.setFillColor(sf::Color::Black);
            dlText.setPosition(70, buttonBaseY + 12);
            m_window.draw(dlText);

            // Create room
            m_createButton.setSize(sf::Vector2f(200, 50));
            m_createButton.setPosition(250, buttonBaseY);
            m_createButton.setFillColor(
                g.installed ? sf::Color(80,150,80) : sf::Color(80,80,80)
            );
            m_window.draw(m_createButton);

            sf::Text crt("Create Room", m_font, 22);
            crt.setFillColor(sf::Color::Black);
            crt.setPosition(280, buttonBaseY + 10);
            m_window.draw(crt);

            // Join room
            m_joinButton.setSize(sf::Vector2f(200, 50));
            m_joinButton.setPosition(480, buttonBaseY);
            m_joinButton.setFillColor(
                g.installed ? sf::Color(80,80,150) : sf::Color(80,80,80)
            );
            m_window.draw(m_joinButton);

            sf::Text jn("Join Room", m_font, 22);
            jn.setFillColor(sf::Color::Black);
            jn.setPosition(510, buttonBaseY + 10);
            m_window.draw(jn);

            // Room code box
            sf::RectangleShape box(sf::Vector2f(200, 40));
            box.setPosition(700, buttonBaseY + 5);
            box.setFillColor(m_joinInputActive ? sf::Color::White : sf::Color(220,220,220));
            box.setOutlineColor(sf::Color::Black);
            box.setOutlineThickness(2);
            m_window.draw(box);

            sf::Text code(m_joinCode.empty() ? "ROOM CODE" : m_joinCode, m_font, 20);
            code.setFillColor(sf::Color::Black);
            code.setPosition(710, buttonBaseY + 10);
            m_window.draw(code);
        }


        drawStatusLine();

        m_reviewOpenBtn.setSize(sf::Vector2f(160, 50));
        m_reviewOpenBtn.setPosition(700, 430);
        m_reviewOpenBtn.setFillColor(sf::Color(150,150,250));
        m_window.draw(m_reviewOpenBtn);

        sf::Text rtxt("Review", m_font, 20);
        rtxt.setFillColor(sf::Color::Black);
        rtxt.setPosition(735, 442);
        m_window.draw(rtxt);

        
        sf::RectangleShape refreshBtn(sf::Vector2f(140, 40));
        refreshBtn.setPosition(700, 20);
        refreshBtn.setFillColor(sf::Color(180,180,180));
        m_window.draw(refreshBtn);

        sf::Text refreshTxt("Refresh", m_font, 18);
        refreshTxt.setFillColor(sf::Color::Black);
        refreshTxt.setPosition(730, 28);
        m_window.draw(refreshTxt);

        m_refreshButton = refreshBtn;

    }


    void drawRoomView() {
        if (!m_room) {
            sf::Text t;
            t.setFont(m_font);
            t.setCharacterSize(24);
            t.setFillColor(sf::Color::White);
            t.setString("Waiting for room info...");
            t.setPosition(40, 40);
            m_window.draw(t);
            drawStatusLine();
            return;
        }

        sf::Text code;
        code.setFont(m_font);
        code.setCharacterSize(20);
        code.setFillColor(sf::Color(200,200,255));
        code.setString("Room Code: " + m_room->roomId);
        code.setPosition(40, 60);
        m_window.draw(code);

        sf::Text title;
        title.setFont(m_font);
        title.setCharacterSize(32);
        title.setFillColor(sf::Color::White);
        title.setString("Room " + m_room->roomId +
                        " - Game ID: " + std::to_string(m_room->gameId));
        title.setPosition(40, 20);
        m_window.draw(title);

        sf::Text codeText;
        codeText.setFont(m_font);
        codeText.setCharacterSize(20);
        codeText.setFillColor(sf::Color(255, 255, 150));
        codeText.setString("Room Code: " + m_room->roomId);
        codeText.setPosition(40, 60);
        m_window.draw(codeText);

        sf::Text info;
        info.setFont(m_font);
        info.setCharacterSize(18);
        info.setFillColor(sf::Color(220, 220, 220));

        std::string line = "Players in room:\n";
        for (auto &name : m_room->players) {
            line += "  - " + name + "\n";
        }
        if (m_room->amHost) {
            line += "\nYou are the host.\n"
                    "Use the buttons below to start the game with\n"
                    "either GUI or CLI client.\n";
        } else {
            line += "\nWaiting for host to start the game...\n";
        }

        info.setString(line);
        info.setPosition(40, 100);
        m_window.draw(info);

        m_roomStartGuiButton.setSize(sf::Vector2f(200, 60));
        m_roomStartGuiButton.setPosition(40, 350);

        m_roomStartCliButton.setSize(sf::Vector2f(200, 60));
        m_roomStartCliButton.setPosition(260, 350);

        if (m_room->amHost) {
            m_roomStartGuiButton.setFillColor(sf::Color(150, 200, 150));
            m_roomStartCliButton.setFillColor(sf::Color(150, 200, 200));
        } else {
            m_roomStartGuiButton.setFillColor(sf::Color(100, 100, 100));
            m_roomStartCliButton.setFillColor(sf::Color(100, 100, 100));
        }
        m_window.draw(m_roomStartGuiButton);
        m_window.draw(m_roomStartCliButton);

        sf::Text startGuiText;
        startGuiText.setFont(m_font);
        startGuiText.setCharacterSize(20);
        startGuiText.setFillColor(sf::Color::Black);
        startGuiText.setString("Start (GUI)");
        startGuiText.setPosition(m_roomStartGuiButton.getPosition().x + 25,
                                 m_roomStartGuiButton.getPosition().y + 18);
        m_window.draw(startGuiText);

        sf::Text startCliText;
        startCliText.setFont(m_font);
        startCliText.setCharacterSize(20);
        startCliText.setFillColor(sf::Color::Black);
        startCliText.setString("Start (CLI)");
        startCliText.setPosition(m_roomStartCliButton.getPosition().x + 25,
                                 m_roomStartCliButton.getPosition().y + 18);
        m_window.draw(startCliText);

        drawStatusLine();
    }

    void drawStatusLine() {
        sf::Text status;
        status.setFont(m_font);
        status.setCharacterSize(16);
        status.setFillColor(
            m_statusIsError
            ? sf::Color(255, 120, 120)    // red for error
            : sf::Color(255, 230, 150)    // yellow-ish for normal
        );
        status.setString(m_statusMessage);
        status.setPosition(500, 620);
        m_window.draw(status);
    }

    void drawReviewPopup() {
        sf::RectangleShape box(sf::Vector2f(600, 300));
        box.setPosition(150, 180);
        box.setFillColor(sf::Color(60,60,80));
        box.setOutlineColor(sf::Color::White);
        box.setOutlineThickness(3);
        m_window.draw(box);

        sf::Text title("Submit Review (1-5)", m_font, 24);
        title.setPosition(180, 200);
        m_window.draw(title);

        sf::Text score("Score: " + std::to_string(m_reviewScore)+"  Change score using up/down", m_font, 20);
        score.setPosition(180, 250);
        m_window.draw(score);

        sf::Text comment("Comment:", m_font, 20);
        comment.setPosition(180, 290);
        m_window.draw(comment);


        sf::RectangleShape inputBox(sf::Vector2f(360, 40));
        inputBox.setPosition(180, 320);
        inputBox.setFillColor(sf::Color::White);
        inputBox.setOutlineColor(sf::Color::Black);
        inputBox.setOutlineThickness(2);
        m_window.draw(inputBox);


        sf::Text input(m_reviewText, m_font, 18);
        input.setFillColor(sf::Color::Black);
        input.setPosition(188, 328);
        m_window.draw(input);


        m_reviewSubmitBtn.setSize(sf::Vector2f(140, 45));
        m_reviewSubmitBtn.setPosition(220, 390);
        m_reviewSubmitBtn.setFillColor(sf::Color(120,200,120));
        m_window.draw(m_reviewSubmitBtn);

        sf::Text yes("Submit", m_font, 18);
        yes.setPosition(260, 400);
        m_window.draw(yes);

        m_reviewCancelBtn.setSize(sf::Vector2f(140, 45));
        m_reviewCancelBtn.setPosition(420, 390);
        m_reviewCancelBtn.setFillColor(sf::Color(200,120,120));
        m_window.draw(m_reviewCancelBtn);

        sf::Text no("Cancel", m_font, 18);
        no.setPosition(460, 400);
        m_window.draw(no);
    }

private:
    // Connection & state
    std::string   m_host;
    int           m_port;
    TCPConnection m_conn;

    bool m_running = false;
    View m_view    = View::Login;

    int  m_playerId = -1;

    std::vector<GameInfo>          m_games;
    int                            m_selectedGameIndex = 0;
    std::optional<RoomInfo>        m_room;
    bool                           m_lastRoomHostFlag = false;
    ClientLaunchMode               m_pendingLaunchMode = ClientLaunchMode::None;

    bool        m_isDownloading   = false;
    std::string m_statusMessage;
    bool        m_statusIsError   = false;

    float m_scrollOffset = 0.f;

    // Login state
    std::string m_loginUser;
    std::string m_loginPass;
    LoginField  m_loginActiveField = LoginField::None;
    float       m_loginSuccessTimer = 0.0f;

    // SFML
    sf::RenderWindow m_window;
    sf::Font         m_font;

    // Login view widgets
    sf::RectangleShape m_loginUserBox;
    sf::RectangleShape m_loginPassBox;
    sf::RectangleShape m_loginButton;
    sf::RectangleShape m_registerButton;

    // Store view widgets
    sf::RectangleShape m_downloadButton;
    sf::RectangleShape m_createButton;
    sf::RectangleShape m_joinButton;
    bool               m_joinInputActive = false;
    std::string        m_joinCode;

    // Room view widgets
    sf::RectangleShape m_roomStartGuiButton;
    sf::RectangleShape m_roomStartCliButton;

    bool        m_reviewBoxOpen = false;
    int         m_reviewScore   = 5;
    std::string m_reviewText;
    std::vector<json> m_currentReviews;
    sf::RectangleShape m_refreshButton;


    // Review buttons
    sf::RectangleShape m_reviewOpenBtn;
    sf::RectangleShape m_reviewSubmitBtn;
    sf::RectangleShape m_reviewCancelBtn;
};


int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Usage: game_store_gui <server_ip> <server_port>\n";
        return 1;
    }

    std::string host = argv[1];
    int         port = std::stoi(argv[2]);

    GameStoreApp app(host, port);
    app.run();
    return 0;
}
