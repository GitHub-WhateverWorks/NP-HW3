#include "game_server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <thread>

using namespace bombarena;

BombArenaServer::BombArenaServer(int port)
    : m_port(port)
{
}

BombArenaServer::~BombArenaServer() {
    closeListenSocket();
}

bool BombArenaServer::setupListenSocket() {
    m_listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenSock < 0) {
        perror("socket");
        return false;
    }

    int opt = 1;
    setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_listenSock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return false;
    }

    if (listen(m_listenSock, 8) < 0) {
        perror("listen");
        return false;
    }

    std::cout << "[Server] Listening on port " << m_port << "\n";
    return true;
}

void BombArenaServer::closeListenSocket() {
    if (m_listenSock >= 0) {
        close(m_listenSock);
        m_listenSock = -1;
    }
}

void BombArenaServer::run() {
    if (!setupListenSocket())
        return;

    // Initialize arena
    m_state = initTwoPlayerDefault();
    m_state.players.clear();
    m_state.bombs.clear();
    m_state.lastExplosionCells.clear();
    m_state.turnNumber = 0;

    acceptLoop();
    closeListenSocket();
}

// ========================================================
// Accept loop
// ========================================================

void BombArenaServer::acceptLoop() {
    std::thread tick(&BombArenaServer::tickLoop, this);
    tick.detach();

    while (m_running) {

        sockaddr_in cli{};
        socklen_t len = sizeof(cli);
        int csock = accept(m_listenSock, (sockaddr *)&cli, &len);
        if (csock < 0) continue;

        // Enforce max players = 3
        {
            std::lock_guard<std::mutex> lk(m_clientsMutex);
            if ((int)m_clients.size() >= m_maxPlayers) {
                std::cout << "[Server] Room full, rejecting.\n";
                close(csock);
                continue;
            }
        }

        int playerId = m_nextPlayerId++;
        auto conn = std::make_shared<TCPConnection>(csock);

        {
            std::lock_guard<std::mutex> lk(m_clientsMutex);
            m_clients.push_back(ClientInfo{playerId, conn, true});
        }

        // Send JOIN signal
        Packet j;
        j.type = PacketType::JOIN_GAME;
        j.data["player_id"] = playerId;
        j.data["max_players"] = m_maxPlayers;
        conn->sendPacket(j);

        std::cout << "[Server] Player #" << playerId << " connected.\n";

        std::thread(&BombArenaServer::clientThread, this, conn, playerId).detach();
    }
}

// ========================================================
// Per-client thread
// ========================================================

void BombArenaServer::clientThread(std::shared_ptr<TCPConnection> conn, int playerId) {

    while (m_running) {
        Packet p;
        if (!conn->recvPacket(p)) {
            std::cout << "[Server] Player #" << playerId << " disconnected.\n";

            std::lock_guard<std::mutex> lk(m_clientsMutex);
            for (auto &c : m_clients)
                if (c.playerId == playerId)
                    c.active = false;
            break;
        }

        switch (p.type) {

            case PacketType::PLAYER_START_GAME:
                startGameIfPossible(playerId);
                break;

            case PacketType::PLAYER_ACTION: {
                std::string s = p.data.value("action", "");
                if (s.empty()) break;

                ActionType act = ActionType::Stay;
                if (s == "w") act = ActionType::MoveUp;
                else if (s == "s") act = ActionType::MoveDown;
                else if (s == "a") act = ActionType::MoveLeft;
                else if (s == "d") act = ActionType::MoveRight;
                else if (s == "b") act = ActionType::PlaceBomb;

                {
                    std::lock_guard<std::mutex> lk(m_pendingMutex);
                    m_pendingActions[playerId] = act;
                }
                break;
            }

            default:
                break;
        }
    }
}

// ========================================================
// Start game
// ========================================================

void BombArenaServer::startGameIfPossible(int requesterId) {
    if (m_gameStarted) return;

    // Only host (player 1)
    if (requesterId != 1) return;

    int count = activePlayerCount();
    if (count < 2) {
        std::cout << "[Server] Need >=2 players to start.\n";
        return;
    }

    // Reset arena
    GameState arena = initTwoPlayerDefault();
    m_state = arena;
    m_state.players.clear();
    m_state.bombs.clear();
    m_state.lastExplosionCells.clear();
    m_state.turnNumber = 0;

    // Spawn points for 1–3 players
    std::vector<std::pair<int,int>> spawns = {
        {1, 1},
        {m_state.width - 2, m_state.height - 2},
        {m_state.width - 2, 1}
    };

    // Collect active players in ascending ID
    std::vector<int> ids;
    {
        std::lock_guard<std::mutex> lk(m_clientsMutex);
        for (auto &c : m_clients)
            if (c.active)
                ids.push_back(c.playerId);
    }
    std::sort(ids.begin(), ids.end());

    for (size_t i = 0; i < ids.size() && i < spawns.size(); i++) {
        PlayerState ps;
        ps.id = ids[i];
        ps.x = spawns[i].first;
        ps.y = spawns[i].second;
        ps.alive = true;
        ps.bombRange = 3;
        m_state.players.push_back(ps);
    }

    m_gameStarted = true;
    std::cout << "[Server] Game started with " << m_state.players.size() << " players.\n";

    Packet s;
    s.type = PacketType::PLAYER_START_GAME;
    broadcastPacket(s);
    broadcastState();
}

// ========================================================
// Tick loop (0.2 seconds)
// ========================================================

void BombArenaServer::tickLoop() {

    while (m_running) {
        usleep(200000);  // 0.2 sec

        if (!m_gameStarted)
            continue;

        std::vector<PlayerAction> acts;

        {
            std::lock_guard<std::mutex> lk(m_pendingMutex);

            if (m_pendingActions.empty()) {
                // Default actions = stay
                for (auto &p : m_state.players)
                    acts.push_back({p.id, ActionType::Stay});
            } else {
                for (auto &p : m_pendingActions)
                    acts.push_back({p.first, p.second});
            }

            m_pendingActions.clear();
        }

        GameResult r = step(m_state, acts);
        broadcastState();

        if (r.type != GameResultType::Ongoing) {
            broadcastGameEnd(r);
            m_running = false;
            break;
        }
    }
}

// ========================================================
// Broadcasting
// ========================================================

int BombArenaServer::activePlayerCount() {
    std::lock_guard<std::mutex> lk(m_clientsMutex);
    int c = 0;
    for (auto &x : m_clients)
        if (x.active) c++;
    return c;
}

void BombArenaServer::broadcastState() {
    Packet s;
    s.type = PacketType::STATE_UPDATE;

    s.data["turn"] = m_state.turnNumber;

    // Players
    nlohmann::json jp = nlohmann::json::array();
    for (auto &p : m_state.players) {
        nlohmann::json t;
        t["id"] = p.id;
        t["x"] = p.x;
        t["y"] = p.y;
        t["alive"] = p.alive;
        jp.push_back(t);
    }
    s.data["players"] = jp;

    // Bombs
    nlohmann::json jb = nlohmann::json::array();
    for (auto &b : m_state.bombs) {
        nlohmann::json t;
        t["x"] = b.x;
        t["y"] = b.y;
        t["timer"] = b.timer;
        t["ownerId"] = b.ownerId;
        t["range"] = b.range;
        jb.push_back(t);
    }
    s.data["bombs"] = jb;

    // Explosions
    nlohmann::json je = nlohmann::json::array();
    for (auto &c : m_state.lastExplosionCells) {
        nlohmann::json t;
        t["x"] = c.first;
        t["y"] = c.second;
        je.push_back(t);
    }
    s.data["explosions"] = je;

    broadcastPacket(s);
}

void BombArenaServer::broadcastGameEnd(const GameResult &r) {
    Packet p;
    p.type = PacketType::GAME_END;

    if (r.type == GameResultType::Draw) {
        p.data["result"] = "draw";
        p.data["winner"] = -1;
    }
    else {
        p.data["result"] = "win";
        p.data["winner"] = r.winnerId;
    }

    broadcastPacket(p);
}

void BombArenaServer::broadcastPacket(const Packet &p) {
    std::lock_guard<std::mutex> lk(m_clientsMutex);
    for (auto &c : m_clients)
        if (c.active)
            c.conn->sendPacket(p);
}
