#pragma once
#include "../shared/tcp.hpp"
#include "../shared/packet.hpp"
#include "../engine/engine.hpp"

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>

class BombArenaServer {

public:
    BombArenaServer(int port);
    ~BombArenaServer();

    void run();

private:

    // =======================================
    // Networking
    // =======================================
    int m_port;
    int m_listenSock = -1;

    bool setupListenSocket();
    void closeListenSocket();

    void acceptLoop();
    void clientThread(std::shared_ptr<TCPConnection> conn, int playerId);

    // =======================================
    // Game state
    // =======================================
    bombarena::GameState m_state;

    bool m_gameStarted = false;
    std::atomic<bool> m_running{true};

    int m_nextPlayerId = 1;
    const int m_maxPlayers = 3;

    struct ClientInfo {
        int playerId;
        std::shared_ptr<TCPConnection> conn;
        bool active;
    };

    std::vector<ClientInfo> m_clients;
    std::mutex m_clientsMutex;

    int activePlayerCount();

    // =======================================
    // Multi-player pending actions per tick
    // =======================================
    std::mutex m_pendingMutex;
    std::unordered_map<int, bombarena::ActionType> m_pendingActions;

    void tickLoop();   // <-- REQUIRED

    // =======================================
    // Game control
    // =======================================
    void startGameIfPossible(int requesterId);

    void broadcastPacket(const Packet &p);
    void broadcastState();
    void broadcastGameEnd(const bombarena::GameResult &res);
};
