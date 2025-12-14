#ifndef DEVELOPER_SERVER_HPP
#define DEVELOPER_SERVER_HPP

#include <vector>
#include <mutex>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <iostream>

#include "../shared/tcp.hpp"
#include "../shared/packet.hpp"
#include "../shared/json.hpp"
using nlohmann::json;

class DeveloperServer {
public:
    explicit DeveloperServer(int port);

    bool start();

    void addHandler(
        PacketType type,
        std::function<void(TCPConnection&, const nlohmann::json&)> handler
    );

private:
    int m_port;

    TCPServer m_server;

    // Store live clients
    std::vector<std::shared_ptr<TCPConnection>> m_clients;
    std::mutex m_clientsMutex;

    std::map<PacketType,
        std::function<void(TCPConnection&, const nlohmann::json&)>> m_handlers;

    void onClient(std::shared_ptr<TCPConnection> conn);
    void sendKeepAlive();
};

#endif

void handleDeveloperLogin(TCPConnection&, const json&);
void handle_register(TCPConnection&, const json&);
void handleListMyGames(TCPConnection&, const json&);
void handleUploadGame(TCPConnection&, const json&);
void handleUpdateGame(TCPConnection&, const json&);
void handleRemoveGame(TCPConnection&, const json&);