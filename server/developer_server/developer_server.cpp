#include "developer_server.hpp"
#include "../database/db.hpp"
#include "../../shared/json.hpp"
#include <thread>
#include <chrono>

using nlohmann::json;

DeveloperServer::DeveloperServer(int port)
    : m_port(port)
{
}

bool DeveloperServer::start() {

    std::cout << "[DeveloperServer] Listening on port " << m_port << "\n";
    addHandler(PacketType::DEV_REGISTER,
        [this](TCPConnection &conn, const json &d) {
            handle_register(conn, d);
        });

    addHandler(PacketType::DEV_LOGIN,
        [this](TCPConnection &conn, const json &d) {
            handleDeveloperLogin(conn, d);
        });

    addHandler(PacketType::DEV_UPLOAD_GAME,
        [this](TCPConnection &conn, const json &d) {
            handleUploadGame(conn, d);
        });

    addHandler(PacketType::DEV_LIST_MY_GAMES,
        [this](TCPConnection &conn, const json &d) {
            handleListMyGames(conn, d);
        });

    addHandler(PacketType::DEV_UPDATE_GAME,
        [this](TCPConnection &conn, const json &d) {
            handleUpdateGame(conn, d);
        });

    addHandler(PacketType::DEV_REMOVE_GAME,
        [this](TCPConnection &conn, const json &d) {
            handleRemoveGame(conn, d);
        });
    bool ok = m_server.start(m_port, [this](TCPConnection conn) {
        auto cptr = std::make_shared<TCPConnection>(std::move(conn));

        {
            std::lock_guard<std::mutex> lk(m_clientsMutex);
            m_clients.push_back(cptr);
        }

        std::thread(&DeveloperServer::onClient, this, cptr).detach();
    });

    if (!ok) return false;

    // periodic keepalive
    std::thread([this]() {
        while (true) {
            sendKeepAlive();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();

    return true;
}

void DeveloperServer::addHandler(
    PacketType type,
    std::function<void(TCPConnection&, const json&)> handler)
{
    m_handlers[type] = std::move(handler);
}

void DeveloperServer::onClient(std::shared_ptr<TCPConnection> conn) {
    std::cout << "[DeveloperServer] New client connected\n";

    Packet packet;
    while (conn->recvPacket(packet)) {
        std::cout << "[DEBUG][SERVER] Received packet type=" 
                << static_cast<int>(packet.type)
                << " json=" << packet.data.dump()
                << "\n";

        auto it = m_handlers.find(packet.type);
        if (it == m_handlers.end()) {
            std::cout << "[DEBUG][SERVER] No handler for this packet type!\n";
        }

        if (it != m_handlers.end()) {
            it->second(*conn, packet.data);
        } else {
            Packet res;
            res.type = PacketType::ERROR_RESPONSE;
            res.data["msg"] = "Unknown developer command.";
            conn->sendPacket(res);
        }
    }

    std::cout << "[DeveloperServer] Client disconnected\n";

    {
        std::lock_guard<std::mutex> lk(m_clientsMutex);
        m_clients.erase(
            std::remove_if(
                m_clients.begin(), m_clients.end(),
                [&](const std::shared_ptr<TCPConnection> &p) {
                    return p.get() == conn.get();
                }),
            m_clients.end()
        );
    }
}

void DeveloperServer::sendKeepAlive() {
    std::lock_guard<std::mutex> lk(m_clientsMutex);

    if (m_clients.empty()) return;

    Packet keep;
    keep.type = PacketType::KEEPALIVE;
    keep.data["msg"] = "ping";

    for (auto &c : m_clients) {
        c->sendPacket(keep);
    }
}
