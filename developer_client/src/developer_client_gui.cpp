#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include "./shared/tcp.hpp"
#include "./shared/packet.hpp"
#include "./shared/protocol.hpp"
#include "./base64.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <thread>
#include <atomic>
#include <chrono>

using nlohmann::json;


static std::string trim(const std::string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

enum class DevView {
    Login,
    Main
};

enum class UploadMode {
    NewGame,
    UpdateExisting
};

enum class ActiveField {
    None,
    LoginUser,
    LoginPass,
    NG_Name,
    NG_Desc,
    NG_Type,
    NG_MaxPlayers,
    NG_Version,
    NG_ZipPath,
    UP_GameId,
    UP_Version,
    UP_ZipPath
};

class DeveloperClientApp {
public:
    DeveloperClientApp(const std::string &host, int port)
        : m_host(host), m_port(port),
          m_window(sf::VideoMode(1000, 720), "Developer Client"),
          m_alive(true)
    {
        if (!m_font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
            std::cerr << "[DevGUI] Could not load font.\n";
        }

        if (!m_conn.connectToServer(host, port)) {
            std::cerr << "[DevGUI] Could not connect.\n";
            m_running = false;
            return;
        }

        m_view = DevView::Login;
        m_running = true;
        m_lastActivity = std::chrono::steady_clock::now();

        //Async network receiver
        m_recvThread = std::thread(&DeveloperClientApp::receiverLoop, this);
    }

    ~DeveloperClientApp() {
        m_alive = false;
        if (m_recvThread.joinable())
            m_recvThread.join();
    }

    void run() {
        sf::Clock clock;

        while (m_window.isOpen() && m_running) {
            float dt = clock.restart().asSeconds();
            (void)dt; // currently unused

            sf::Event ev;
            while (m_window.pollEvent(ev)) {
                if (ev.type == sf::Event::Closed) {
                    m_window.close();
                    m_running = false;
                } else {
                    handleEvent(ev);
                }
            }

            draw();
        }
    }

private:
    
    void receiverLoop() {
        while (m_alive) {
            Packet p;
            if (!m_conn.recvPacket(p)) {
                // Hard disconnect
                m_statusText = "Lost connection to server!";
                m_running = false;
                m_alive = false;
                return;
            }

            m_lastActivity = std::chrono::steady_clock::now();

            if (p.type == PacketType::KEEPALIVE) {
                // heartbeat only
                continue;
            }

            handlePacket(p);
        }
    }

    void handlePacket(const Packet &p) {
        if (p.type == PacketType::SERVER_RESPONSE) {
            handleServerResponse(p.data);
        }
    }

    void handleServerResponse(const json &d) {
        std::cout << "[DEBUG][GUI] Received SERVER_RESPONSE: " << d.dump() << "\n";

        bool ok         = d.value("ok", true);
        std::string kind = d.value("kind", "");

        if (!ok) {
            m_statusText = "Server error: " + d.value("msg", std::string("unknown"));
            return;
        }

        if (kind == "DEV_LOGIN") {
            m_devId = d.value("dev_id", -1);
            if (*m_devId < 0) {
                m_statusText = "Login OK but dev_id missing/invalid.";
                return;
            }
            m_view = DevView::Main;
            m_statusText = "Login successful.";
            Packet req;
            req.type = PacketType::DEV_LIST_MY_GAMES;
            req.data["dev_id"] = *m_devId;
            m_conn.sendPacket(req);
        }
        else if (kind == "DEV_REGISTER") {
            if (d.contains("dev_id")) {
                m_devId = d["dev_id"].get<int>();
                m_view = DevView::Main;
                m_statusText = "Register & login successful.";
            } else {
                m_statusText = "Register successful. Now you can login.";
            }
        }
        else if (kind == "DEV_UPLOAD_GAME") {
            m_statusText =
                "Upload OK! game_id=" +
                std::to_string(d.value("game_id", -1)) +
                " version=" + d.value("version_str", std::string("?"));
            if (m_devId) {
                Packet req;
                req.type = PacketType::DEV_LIST_MY_GAMES;
                req.data["dev_id"] = *m_devId;
                m_conn.sendPacket(req);
            }
        }
        else if (kind == "DEV_LIST_MY_GAMES") {
            m_devGames.clear();
            for (const auto &g : d["games"]) {
                DevGameInfo info;
                info.id     = g.value("game_id", -1);
                info.name   = g.value("name", "<noname>");
                info.active = g.value("active", true);
                m_devGames.push_back(info);
            }
            m_statusText = "Loaded game list.";
        }
        else if (kind == "DEV_REMOVE_GAME") {
            m_statusText = "Game unpublished successfully.";

            if (m_devId) {
                Packet req;
                req.type = PacketType::DEV_LIST_MY_GAMES;
                req.data["dev_id"] = *m_devId;
                m_conn.sendPacket(req);
            }
        }
        else {
            m_statusText = "Server: " + kind;
        }
    }

    void sendLogin() {
        if (m_loginUser.empty() || m_loginPass.empty()) {
            m_statusText = "Username/password required.";
            return;
        }

        Packet p;
        p.type = PacketType::DEV_LOGIN;
        p.data["username"] = m_loginUser;
        p.data["password"] = m_loginPass;

        if (!m_conn.sendPacket(p)) {
            m_statusText = "Failed to send login packet.";
            return;
        }
        m_statusText = "Logging in...";
    }

    void sendRegister() {
        std::cout << "[DEBUG][GUI] sendRegister(): username=" << m_loginUser
                << " pass=" << m_loginPass << "\n";

        if (m_loginUser.empty() || m_loginPass.empty()) {
            m_statusText = "Username/password required.";
            std::cout << "[DEBUG][GUI] Missing fields\n";
            return;
        }

        Packet p;
        p.type = PacketType::DEV_REGISTER;
        p.data["username"] = m_loginUser;
        p.data["password"] = m_loginPass;

        std::cout << "[DEBUG][GUI] Sending register packet: "
                << p.serialize() << "\n";

        if (!m_conn.sendPacket(p)) {
            m_statusText = "Failed to send register packet.";
            std::cout << "[DEBUG][GUI] sendPacket FAILED\n";
            return;
        }

        m_statusText = "Registering developer...";
        std::cout << "[DEBUG][GUI] Packet sent OK\n";
    }

    void sendUploadNewGame() {
        if (!m_devId) {
            m_statusText = "Login first.";
            return;
        }

        if (m_newName.empty() || m_newDesc.empty() || m_newType.empty() ||
            m_newMaxPlayers.empty() || m_newZip.empty()) {
            m_statusText = "Missing fields for new game.";
            return;
        }

        std::ifstream fin(m_newZip, std::ios::binary);
        if (!fin) {
            m_statusText = "Cannot open ZIP: " + m_newZip;
            return;
        }
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(fin)),
                                  std::istreambuf_iterator<char>());
        std::string b64 = encodeBase64(raw);

        Packet p;
        p.type = PacketType::DEV_UPLOAD_GAME;
        p.data["dev_id"]        = *m_devId;
        p.data["game_name"]     = m_newName;
        p.data["description"]   = m_newDesc;
        p.data["game_type"]     = m_newType;
        p.data["max_players"]   = std::stoi(m_newMaxPlayers);
        p.data["version_str"]   = (m_newVersion.empty() ? "v1.0" : m_newVersion);
        p.data["filename"]      = "game.zip";
        p.data["filedata_base64"] = b64;

        if (!m_conn.sendPacket(p)) {
            m_statusText = "Failed to send upload (new game).";
            return;
        }
        m_statusText = "Uploading new game...";
    }

    void sendUploadUpdate() {
        if (!m_devId) {
            m_statusText = "Login first.";
            return;
        }

        if (m_upGameId.empty() || m_upVersion.empty() || m_upZip.empty()) {
            m_statusText = "Missing fields for update.";
            return;
        }

        std::ifstream fin(m_upZip, std::ios::binary);
        if (!fin) {
            m_statusText = "Cannot open ZIP: " + m_upZip;
            return;
        }
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(fin)),
                                  std::istreambuf_iterator<char>());
        std::string b64 = encodeBase64(raw);

        Packet p;
        p.type = PacketType::DEV_UPLOAD_GAME;
        p.data["dev_id"]        = *m_devId;
        p.data["game_id"]       = std::stoi(m_upGameId);
        p.data["version_str"]   = m_upVersion;
        p.data["filename"]      = "game.zip";
        p.data["filedata_base64"] = b64;

        if (!m_conn.sendPacket(p)) {
            m_statusText = "Failed to send upload (update).";
            return;
        }
        m_statusText = "Uploading update...";
    }

    void handleEvent(const sf::Event &ev) {
        if (m_view == DevView::Login)
            handleLoginEvent(ev);
        else
            handleMainEvent(ev);
    }

    void handleLoginEvent(const sf::Event &ev) {
        if (ev.type == sf::Event::MouseButtonPressed) {
            sf::Vector2f m(ev.mouseButton.x, ev.mouseButton.y);

            if (m_loginUserBox.getGlobalBounds().contains(m))
                m_active = ActiveField::LoginUser;
            else if (m_loginPassBox.getGlobalBounds().contains(m))
                m_active = ActiveField::LoginPass;
            else if (m_loginBtn.getGlobalBounds().contains(m))
                sendLogin();
            else if (m_registerBtn.getGlobalBounds().contains(m)){
                std::cout << "[DEBUG][GUI] Register button clicked\n";
                sendRegister();
            }else
                m_active = ActiveField::None;
        }
        else if (ev.type == sf::Event::TextEntered) {
            applyTextInput(ev.text.unicode);
        }
    }

    void handleMainEvent(const sf::Event &ev) {
        if (m_confirmRemoveOpen && ev.type == sf::Event::MouseButtonPressed) {
            sf::Vector2f m(ev.mouseButton.x, ev.mouseButton.y);

            if (m_confirmYesBtn.getGlobalBounds().contains(m)) {
                Packet p;
                p.type = PacketType::DEV_REMOVE_GAME;
                p.data["dev_id"]  = *m_devId;
                p.data["game_id"] = m_confirmRemoveGameId;

                m_conn.sendPacket(p);
                m_statusText = "Unpublishing game...";
                m_confirmRemoveOpen = false;
                return;
            }

            if (m_confirmNoBtn.getGlobalBounds().contains(m)) {
                m_confirmRemoveOpen = false;
                m_statusText = "Cancelled.";
                return;
            }
            return;
        }
        if (ev.type != sf::Event::MouseButtonPressed) {
            if (ev.type == sf::Event::TextEntered)
                applyTextInput(ev.text.unicode);
            return;
        }

        sf::Vector2f m(ev.mouseButton.x, ev.mouseButton.y);

        if (m_modeNewBtn.getGlobalBounds().contains(m)) {
            m_uploadMode = UploadMode::NewGame;
            return;
        }
        if (m_modeUpdateBtn.getGlobalBounds().contains(m)) {
            m_uploadMode = UploadMode::UpdateExisting;
            return;
        }

        if (m_uploadMode == UploadMode::UpdateExisting) {
            if (m_backBtn.getGlobalBounds().contains(m)) {
                m_uploadMode = UploadMode::NewGame;
                return;
            }
        }

        if (m_browseZipBtn.getGlobalBounds().contains(m)) {
            std::string path = openFileDialog();
            if (!path.empty()) {
                if (m_uploadMode == UploadMode::NewGame)
                    m_newZip = path;
                else
                    m_upZip = path;
            }
            return;
        }

        if (m_uploadMode == UploadMode::UpdateExisting) {
            float ly = 180;
            for (auto &g : m_devGames) {
                sf::FloatRect r(650, ly, 300, 26); 
                if (r.contains(m)) {
                    m_upGameId = std::to_string(g.id);
                    m_active = ActiveField::UP_GameId;
                    return;
                }
                ly += 26;
            }
        }


        if (m_uploadMode == UploadMode::NewGame) {
            if (m_newNameBox.getGlobalBounds().contains(m)) m_active = ActiveField::NG_Name;
            else if (m_newDescBox.getGlobalBounds().contains(m)) m_active = ActiveField::NG_Desc;
            else if (m_newTypeBox.getGlobalBounds().contains(m)) m_active = ActiveField::NG_Type;
            else if (m_newMaxPlayersBox.getGlobalBounds().contains(m)) m_active = ActiveField::NG_MaxPlayers;
            else if (m_newVersionBox.getGlobalBounds().contains(m)) m_active = ActiveField::NG_Version;
            else if (m_newZipBox.getGlobalBounds().contains(m)) m_active = ActiveField::NG_ZipPath;
            else if (m_uploadBtn.getGlobalBounds().contains(m)) {
                sendUploadNewGame();
            }
        }
        else { 
            if (m_upGameIdBox.getGlobalBounds().contains(m)) m_active = ActiveField::UP_GameId;
            else if (m_upVersionBox.getGlobalBounds().contains(m)) m_active = ActiveField::UP_Version;
            else if (m_upZipBox.getGlobalBounds().contains(m)) m_active = ActiveField::UP_ZipPath;
            else if (m_uploadBtn.getGlobalBounds().contains(m)) {
                sendUploadUpdate();
            }
        }

        if (m_uploadMode == UploadMode::UpdateExisting) {
            if (m_removeBtn.getGlobalBounds().contains(m)) {
                if (!m_devId || m_upGameId.empty()) {
                    m_statusText = "Select a game first.";
                    return;
                }

                m_confirmRemoveGameId = std::stoi(m_upGameId);
                m_confirmRemoveOpen = true;
                return;
            }
        }

    }



    void applyTextInput(sf::Uint32 u) {
        if (u == '\b') {
            std::string *f = currentField();
            if (f && !f->empty()) f->pop_back();
            return;
        }
        if (u < 32 || u > 126) return;
        char c = static_cast<char>(u);

        std::string *f = currentField();
        if (f) f->push_back(c);
    }

    std::string* currentField() {
        switch (m_active) {
            case ActiveField::LoginUser: return &m_loginUser;
            case ActiveField::LoginPass: return &m_loginPass;
            case ActiveField::NG_Name: return &m_newName;
            case ActiveField::NG_Desc: return &m_newDesc;
            case ActiveField::NG_Type: return &m_newType;
            case ActiveField::NG_MaxPlayers: return &m_newMaxPlayers;
            case ActiveField::NG_Version: return &m_newVersion;
            case ActiveField::NG_ZipPath: return &m_newZip;
            case ActiveField::UP_GameId: return &m_upGameId;
            case ActiveField::UP_Version: return &m_upVersion;
            case ActiveField::UP_ZipPath: return &m_upZip;
            default: return nullptr;
        }
    }


    void draw() {
        m_window.clear(sf::Color(38, 42, 55));

        if (m_view == DevView::Login) drawLogin();
        else                         drawMain();

        drawStatus();
        
        if (m_confirmRemoveOpen)
            drawRemoveConfirm();
        m_window.display();
    }

    void drawLogin() {
        sf::Text title("Developer Login", m_font, 32);
        title.setFillColor(sf::Color::White);
        title.setPosition(50, 40);
        m_window.draw(title);

        drawField(m_loginUserBox, "Username", m_loginUser, 120);
        drawField(m_loginPassBox, "Password", std::string(m_loginPass.size(), '*'), 190);

        // Login button
        m_loginBtn.setPosition(50, 260);
        m_loginBtn.setSize(sf::Vector2f(180, 45));
        m_loginBtn.setFillColor(sf::Color(100, 160, 100));
        m_window.draw(m_loginBtn);

        sf::Text tLogin("Login", m_font, 20);
        tLogin.setFillColor(sf::Color::Black);
        tLogin.setPosition(95, 270);
        m_window.draw(tLogin);

        // Register button 
        m_registerBtn.setPosition(250, 260);
        m_registerBtn.setSize(sf::Vector2f(180, 45));
        m_registerBtn.setFillColor(sf::Color(120, 150, 230));
        m_window.draw(m_registerBtn);

        sf::Text tReg("Register", m_font, 20);
        tReg.setFillColor(sf::Color::Black);
        tReg.setPosition(285, 270);
        m_window.draw(tReg);
    }

    void drawMain() {
        sf::Text title("Developer Panel", m_font, 28);
        title.setFillColor(sf::Color::White);
        title.setPosition(40, 30);
        m_window.draw(title);


        m_modeUpdateBtn.setPosition(280, 90);
        m_modeUpdateBtn.setSize(sf::Vector2f(260, 45));
        m_modeUpdateBtn.setFillColor(
            m_uploadMode == UploadMode::UpdateExisting ?
            sf::Color(140, 200, 140) : sf::Color(200, 200, 200));
        m_window.draw(m_modeUpdateBtn);


        sf::Text tu("My Games", m_font, 18);
        tu.setFillColor(sf::Color::Black);
        tu.setPosition(295, 101);
        m_window.draw(tu);

        if (m_uploadMode == UploadMode::NewGame)
            drawNewGameForm();
        else
            drawUpdateForm();
    }

    void drawNewGameForm() {
        float y = 180;

        drawField(m_newNameBox,        "Game Name",    m_newName,         y); y += 70;
        drawField(m_newDescBox,        "Description",  m_newDesc,         y); y += 70;
        drawField(m_newTypeBox,        "Game Type",    m_newType,         y); y += 70;
        drawField(m_newMaxPlayersBox,  "Max Players",  m_newMaxPlayers,   y); y += 70;
        drawField(m_newVersionBox,     "Version",      m_newVersion,      y); y += 70;
        drawField(m_newZipBox,         "Path to ZIP",  m_newZip,          y);

        m_browseZipBtn.setPosition(560, y);    
        m_browseZipBtn.setSize(sf::Vector2f(120, 40));
        m_browseZipBtn.setFillColor(sf::Color(180, 180, 220));
        m_window.draw(m_browseZipBtn);

        sf::Text bt("Browse...", m_font, 16);
        bt.setFillColor(sf::Color::Black);
        bt.setPosition(575, y + 8);
        m_window.draw(bt);


        m_uploadBtn.setPosition(40, y + 70);
        m_uploadBtn.setSize(sf::Vector2f(240, 50));
        m_uploadBtn.setFillColor(sf::Color(120, 150, 230));
        m_window.draw(m_uploadBtn);

        sf::Text t("Upload NEW Game", m_font, 20);
        t.setFillColor(sf::Color::Black);
        t.setPosition(50, y + 82);
        m_window.draw(t);
    }

    void drawUpdateForm() {
        float yn = 140;

        m_backBtn.setPosition(180, yn);
        m_backBtn.setSize(sf::Vector2f(180, 40));
        m_backBtn.setFillColor(sf::Color(200, 140, 140));
        m_window.draw(m_backBtn);

        sf::Text bt("Back", m_font, 18);
        bt.setFillColor(sf::Color::Black);
        bt.setPosition(180, yn + 8);
        m_window.draw(bt);

        yn += 60;
        float lx = 650;      
        float ly = 180;
        sf::Text listTitle("Your Games:", m_font, 20);
        listTitle.setFillColor(sf::Color::White);
        listTitle.setPosition(lx, 150);
        m_window.draw(listTitle);
        for (auto &g : m_devGames) {
            std::string status = g.active ? "(active)" : "(inactive)";
            std::string line =
                "ID: " + std::to_string(g.id) + "   " + g.name + " " + status;

            sf::Text t(line, m_font, 16);
            t.setFillColor(sf::Color(200, 200, 200));
            t.setPosition(lx, ly);
            m_window.draw(t);

            ly += 26;
        }

        float y = 180;

        drawField(m_upGameIdBox, "Game ID", m_upGameId, y); y += 70;
        drawField(m_upVersionBox, "Version", m_upVersion, y); y += 70;
        drawField(m_upZipBox, "Path to ZIP", m_upZip, y);

        m_uploadBtn.setPosition(40, y + 70);
        m_uploadBtn.setSize(sf::Vector2f(200, 50));
        m_uploadBtn.setFillColor(sf::Color(120, 150, 230));
        m_window.draw(m_uploadBtn);

        sf::Text t("Upload Update", m_font, 20);
        t.setFillColor(sf::Color::Black);
        t.setPosition(70, y + 82);
        m_window.draw(t);

        m_removeBtn.setPosition(260, y + 70);
        m_removeBtn.setSize(sf::Vector2f(200, 50));
        m_removeBtn.setFillColor(sf::Color(200, 100, 100));
        m_window.draw(m_removeBtn);

        sf::Text tr("Unpublish Game", m_font, 20);
        tr.setFillColor(sf::Color::Black);
        tr.setPosition(280, y + 82);
        m_window.draw(tr);

    }

    void drawField(sf::RectangleShape &box, const std::string &label,
                   const std::string &content, float y) {
        box.setSize(sf::Vector2f(500, 40));
        box.setPosition(40, y);
        box.setFillColor(sf::Color(230, 230, 230));
        box.setOutlineColor(sf::Color::Black);
        box.setOutlineThickness(2);
        m_window.draw(box);

        sf::Text l(label, m_font, 16);
        l.setFillColor(sf::Color::White);
        l.setPosition(40, y - 25);
        m_window.draw(l);

        sf::Text t(content, m_font, 18);
        t.setFillColor(sf::Color::Black);
        t.setPosition(50, y + 8);
        m_window.draw(t);
    }

    void drawStatus() {
        sf::RectangleShape bar(sf::Vector2f(1000, 30));
        bar.setPosition(0, 690);
        bar.setFillColor(sf::Color(45, 45, 55));
        m_window.draw(bar);

        sf::Text t(m_statusText, m_font, 14);
        t.setFillColor(sf::Color(230, 230, 230));
        t.setPosition(10, 695);
        m_window.draw(t);
    }

    std::string openFileDialog() {
        FILE *fp = popen("zenity --file-selection --title=\"Select ZIP\" 2>/dev/null", "r");
        if (!fp) return "";

        char buff[4096];
        std::string result = "";

        if (fgets(buff, sizeof(buff), fp))
            result = std::string(buff);

        pclose(fp);

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        return result;
    }

    void drawRemoveConfirm() {
        sf::RectangleShape box;
        box.setSize(sf::Vector2f(600, 220));
        box.setPosition(200, 250);
        box.setFillColor(sf::Color(50, 50, 60));
        box.setOutlineColor(sf::Color::Red);
        box.setOutlineThickness(3);
        m_window.draw(box);

        sf::Text t1("Unpublish Game Confirmation", m_font, 22);
        t1.setPosition(230, 265);
        t1.setFillColor(sf::Color::White);
        m_window.draw(t1);

        sf::Text t2(
            "This game will:\n"
            "- Disappear from player store\n"
            "- Reject new rooms\n"
            "- Not affect existing rooms\n\n"
            "Are you sure?",
            m_font, 18
        );
        t2.setPosition(230, 300);
        t2.setFillColor(sf::Color::White);
        m_window.draw(t2);


        m_confirmYesBtn.setSize(sf::Vector2f(120, 45));
        m_confirmYesBtn.setPosition(300, 420);
        m_confirmYesBtn.setFillColor(sf::Color(120, 200, 120));
        m_window.draw(m_confirmYesBtn);

        sf::Text yes("YES", m_font, 18);
        yes.setFillColor(sf::Color::Black);
        yes.setPosition(340, 430);
        m_window.draw(yes);


        m_confirmNoBtn.setSize(sf::Vector2f(120, 45));
        m_confirmNoBtn.setPosition(480, 420);
        m_confirmNoBtn.setFillColor(sf::Color(200, 120, 120));
        m_window.draw(m_confirmNoBtn);

        sf::Text no("CANCEL", m_font, 18);
        no.setFillColor(sf::Color::Black);
        no.setPosition(500, 430);
        m_window.draw(no);
    }


private:

    std::string     m_host;
    int             m_port;
    TCPConnection   m_conn;

    std::thread       m_recvThread;
    std::atomic<bool> m_alive;
    std::chrono::steady_clock::time_point m_lastActivity;

    bool        m_running = false;
    DevView     m_view    = DevView::Login;

    std::optional<int> m_devId;

    UploadMode  m_uploadMode = UploadMode::NewGame;
    ActiveField m_active     = ActiveField::None;

    struct DevGameInfo {
        int id;
        std::string name;
        bool active;
    };

    std::vector<DevGameInfo> m_devGames;   
    bool m_confirmRemoveOpen = false;
    int  m_confirmRemoveGameId = -1;


    sf::RenderWindow m_window;
    sf::Font         m_font;

    // UI components
    sf::RectangleShape m_loginUserBox, m_loginPassBox;
    sf::RectangleShape m_loginBtn, m_registerBtn;
    sf::RectangleShape m_modeNewBtn, m_modeUpdateBtn, m_uploadBtn;

    // NEW GAME fields
    sf::RectangleShape m_newNameBox, m_newDescBox, m_newTypeBox;
    sf::RectangleShape m_newMaxPlayersBox, m_newVersionBox, m_newZipBox;
    sf::RectangleShape m_browseZipBtn;
    sf::RectangleShape m_backBtn;



    std::string m_newName, m_newDesc, m_newType, m_newMaxPlayers;
    std::string m_newVersion, m_newZip;

    // UPDATE fields
    sf::RectangleShape m_upGameIdBox, m_upVersionBox, m_upZipBox;
    std::string m_upGameId, m_upVersion, m_upZip;
    sf::RectangleShape m_removeBtn;
    sf::RectangleShape m_confirmYesBtn, m_confirmNoBtn;


    // LOGIN
    std::string m_loginUser, m_loginPass;

    // Status
    std::string m_statusText = "Not logged in.";
};


int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Usage: developer_client_gui <server_ip> <port>\n";
        return 1;
    }

    DeveloperClientApp app(argv[1], std::stoi(argv[2]));
    app.run();
    return 0;
}
