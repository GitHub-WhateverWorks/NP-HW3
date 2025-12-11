#pragma once
#ifndef LOBBY_SERVER_HPP
#define LOBBY_SERVER_HPP

#include "../shared/json.hpp"
#include "../shared/tcp.hpp"
#include "../shared/protocol.hpp"

#include <unordered_map>
#include <vector>
#include <functional>

using json = nlohmann::json;

struct Room {
    int roomId;
    int gameId;
    int hostPlayerId;
    int maxPlayers;
    std::vector<int> players;

    // Game server process tracking
    pid_t serverPid = -1;
    bool  serverRunning = false;
};

class LobbyServer {
public:
    explicit LobbyServer(int port);

    bool start();

    // main per-connection loop
    void onClient(TCPConnection conn);

    using HandlerFunc = std::function<void(TCPConnection&, const json&)>;
    void addHandler(PacketType type, HandlerFunc func);

    // Rooms
    int  createRoom(int gameId, int hostPlayerId, int maxPlayers);
    Room* getRoom(int roomId);

    // Disconnect handling
    void handlePlayerDisconnect(int playerId);
    Room* findRoomByPlayer(int playerId);
    void  removeRoom(int roomId);

    bool isPlayerOnline(int playerId) const;
    void registerPlayer(int playerId, int fd);
    // Player <-> fd mapping
    int  getPlayerIdByFd(int fd);
    void unregisterPlayer(int fd);

    // These are used by handlers 
    std::unordered_map<int,int> m_fdToPlayer;   
    std::unordered_map<int,int> m_playerToFd;   

    // rooms
    std::unordered_map<int, Room> m_rooms;
    int allocateGamePort();
    bool sendByFd(int fd, const Packet &p);
private:
    int m_port;
    TCPServer m_server;

    std::unordered_map<PacketType, HandlerFunc> m_handlers;

    int nextRoomId = 1;
};



void handlePlayerRegister(TCPConnection&, const nlohmann::json&);
void handlePlayerLogin(TCPConnection&, const nlohmann::json&);
void handleListGames(TCPConnection&, const nlohmann::json&);
void handleDownloadGame(TCPConnection&, const nlohmann::json&);
void handleCreateRoom(TCPConnection&, const nlohmann::json&);
void handleJoinRoom(TCPConnection&, const nlohmann::json&);
void handleStartGame(TCPConnection&, const nlohmann::json&);
void handleSubmitReview(TCPConnection &conn, const nlohmann::json &d);
void handleGetReviews(TCPConnection &conn, const nlohmann::json &d);

#endif