#include "lobby_server.hpp"
#include "../database/db.hpp"
#include <iostream>
#include <algorithm>
#include <signal.h>     
#include <unistd.h>     

LobbyServer::LobbyServer(int port)
    : m_port(port)
{
}

bool LobbyServer::start() {

    std::cout << "[LobbyServer] Listening on port " << m_port << "\n";

    addHandler(PacketType::PLAYER_REGISTER,
        [this](TCPConnection &conn, const json &d) {
            handlePlayerRegister(conn, d);
        });

    addHandler(PacketType::PLAYER_LOGIN,
        [this](TCPConnection &conn, const json &d) {
            handlePlayerLogin(conn, d);
        });

    addHandler(PacketType::PLAYER_LIST_GAMES,
        [this](TCPConnection &conn, const json &d) {
            handleListGames(conn, d);
        });

    addHandler(PacketType::PLAYER_DOWNLOAD_GAME,
        [this](TCPConnection &conn, const json &d) {
            handleDownloadGame(conn, d);
        });

    addHandler(PacketType::PLAYER_CREATE_ROOM,
        [this](TCPConnection &conn, const json &d) {
            handleCreateRoom(conn, d);
        });

    addHandler(PacketType::PLAYER_JOIN_ROOM,
        [this](TCPConnection &conn, const json &d) {
            handleJoinRoom(conn, d);
        });

    addHandler(PacketType::PLAYER_START_GAME,
        [this](TCPConnection &conn, const json &d) {
            handleStartGame(conn, d);
        });
    addHandler(PacketType::PLAYER_SUBMIT_REVIEW,
        [this](TCPConnection &conn, const json &d) {
            handleSubmitReview(conn, d);
        });

    addHandler(PacketType::PLAYER_GET_REVIEWS,
        [this](TCPConnection &conn, const json &d) {
            handleGetReviews(conn, d);
        });
    return m_server.start(m_port, [this](TCPConnection conn) {
        conn.owner = this;     
        std::thread(&LobbyServer::onClient, this, std::move(conn)).detach();
    });
}

void LobbyServer::addHandler(PacketType type, HandlerFunc func) {
    m_handlers[type] = std::move(func);
}

// ---------------------------------------------------------
// Rooms
// ---------------------------------------------------------

int LobbyServer::createRoom(int gameId, int hostPlayerId, int maxPlayers) {
    Room room;
    room.roomId      = nextRoomId++;
    room.gameId      = gameId;
    room.hostPlayerId= hostPlayerId;
    room.maxPlayers  = maxPlayers;
    room.players.push_back(hostPlayerId);

    m_rooms[room.roomId] = room;

    std::cout << "[Lobby] Created room " << room.roomId
              << " for game " << gameId
              << " host player " << hostPlayerId << "\n";

    return room.roomId;
}

Room* LobbyServer::getRoom(int roomId) {
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end()) return nullptr;
    return &it->second;
}


void LobbyServer::onClient(TCPConnection conn) {
    std::cout << "[LobbyServer] New client connected\n";

    int fd = conn.fd();
    Packet packet;

    while (conn.recvPacket(packet)) {
        std::cout << "[LobbyServer] Received packet type="
                  << static_cast<int>(packet.type)
                  << " json=" << packet.data.dump()
                  << "\n";

        auto it = m_handlers.find(packet.type);
        if (it != m_handlers.end()) {
            it->second(conn, packet.data);
        } else {
            Packet res;
            res.type = PacketType::ERROR_RESPONSE;
            res.data["msg"] = "Unknown lobby command!. ";
            conn.sendPacket(res);
        }
    }

    std::cout << "[LobbyServer] Client disconnected\n";

    int playerId = getPlayerIdByFd(fd);

    unregisterPlayer(fd);

    if (playerId > 0) {
        handlePlayerDisconnect(playerId);
    }
}


void LobbyServer::handlePlayerDisconnect(int playerId) {
    Room* room = findRoomByPlayer(playerId);
    if (!room) return;

    auto &v = room->players;
    v.erase(std::remove(v.begin(), v.end(), playerId), v.end());

    if (v.empty()) {
        if (room->serverRunning && room->serverPid > 0) {
            kill(room->serverPid, SIGKILL);
            std::cout << "[Lobby] Killed game server PID " << room->serverPid << "\n";
        }
        removeRoom(room->roomId);
        return;
    }

    std::cout << "[Lobby] Player " << playerId
              << " left, " << v.size() << " players remain. Server stays alive.\n";
}


Room* LobbyServer::findRoomByPlayer(int playerId) {
    for (auto &kv : m_rooms) {
        Room &room = kv.second;
        for (int pid : room.players) {
            if (pid == playerId)
                return &kv.second;
        }
    }
    return nullptr;
}

void LobbyServer::removeRoom(int roomId) {
    m_rooms.erase(roomId);
    std::cout << "[Lobby] Removed room " << roomId << "\n";
}

bool LobbyServer::isPlayerOnline(int playerId) const {
    return m_playerToFd.find(playerId) != m_playerToFd.end();
}

void LobbyServer::registerPlayer(int playerId, int fd) {
    m_playerToFd[playerId] = fd;
    m_fdToPlayer[fd]       = playerId;
}

int LobbyServer::getPlayerIdByFd(int fd) {
    auto it = m_fdToPlayer.find(fd);
    if (it == m_fdToPlayer.end()) return -1;
    return it->second;
}

void LobbyServer::unregisterPlayer(int fd) {
    int pid = getPlayerIdByFd(fd);
    if (pid > 0)
        m_playerToFd.erase(pid);
    m_fdToPlayer.erase(fd);
}
int LobbyServer::allocateGamePort() {
    static int nextPort = 20100;  


    return nextPort++;
}