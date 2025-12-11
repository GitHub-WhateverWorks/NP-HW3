#include "../lobby_server.hpp"
#include "../../database/db.hpp"
#include <signal.h>
#include <unistd.h>
#include <filesystem>
void handleStartGame(TCPConnection &conn, const nlohmann::json &d) {
    std::cout<<"Start handling start game\n";
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "START_GAME";

    if (!d.contains("room_id") || !d.contains("player_id")) {
        r.data["ok"] = false;
        r.data["msg"] = "Missing room_id/player_id.";
        conn.sendPacket(r);
        return;
    }

    int roomId = d["room_id"];
    int playerId = d["player_id"];

    auto *server = reinterpret_cast<LobbyServer*>(conn.owner);
    Room *room = server->getRoom(roomId);
    std::cout<<"check room\n";
    if (!room) {
        r.data["ok"] = false;
        r.data["msg"] = "Room not found.";
        conn.sendPacket(r);
        return;
    }
    std::cout<<"check host\n";
    // Host only
    if (room->hostPlayerId != playerId) {
        r.data["ok"] = false;
        r.data["msg"] = "Only host can start the game.";
        conn.sendPacket(r);
        return;
    }
    std::cout<<"check player count\n";
    // At least 2 players
    if (room->players.size() < 2) {
        r.data["ok"] = false;
        r.data["msg"] = "Need at least 2 players.";
        conn.sendPacket(r);
        return;
    }


    std::string base = Database::instance().getLatestVersionStoragePath(room->gameId);
    if (base.back() != '/') base += '/';

    std::string serverDir = base + "server/";


    std::string serverExe;
    namespace fs = std::filesystem;
    std::cout<<"serverDir: "<<serverDir<<"\n";
    if (fs::exists(serverDir)) {
        for (auto &entry : fs::directory_iterator(serverDir)) {
            if (!entry.is_regular_file()) continue;

            std::string name = entry.path().filename().string();
            if (name == "game_server") {
                serverExe = entry.path().string();
                break;
            }
        }

    }

    if (serverExe.empty()) {
        r.data["ok"] = false;
        r.data["msg"] = "No executable found in: " + serverDir;
        conn.sendPacket(r);
        return;
    }
    int port = server->allocateGamePort();
    //int port = 20000;

    pid_t pid = fork();
    if (pid == 0) {
        std::cout<<"handle_start_game fork\n";
        execl(serverExe.c_str(),
            serverExe.c_str(),
            "--port",
            std::to_string(port).c_str(),
            (char*)NULL);
        std::cout<<"handle_start_game fork finished exe\n";
        std::cerr << "[Lobby] exec() failed for " << serverExe << "\n";
        _exit(1);
    }

    room->serverPid = pid;
    room->serverRunning = true;

    std::cout << "[Lobby] Game server PID=" << pid
              << " on port " << port << "\n";

    std::cout<<"broadcasting\n";
    Packet b;
    b.type = PacketType::SERVER_RESPONSE;
    b.data["kind"] = "START_GAME";
    b.data["ok"] = true;
    b.data["game_id"] = room->gameId;
    b.data["room_id"] = room->roomId;
    b.data["server_port"] = port;
    for (int pidPlayer : room->players) {
        std::cout<<"broadcasting for player: "<<pidPlayer<<"\n";
        Packet b2 = b;
        b2.data["is_host"] = (pidPlayer == room->hostPlayerId)? "1":"0";
        if (pidPlayer == room->hostPlayerId) std::cout<< "Found Host!\n";
        int fd = server->m_playerToFd[pidPlayer];
        if (fd <= 0) continue;
        server->sendByFd(fd, b2);
    }
    std::cout<<"broadcasting finished\n";
}
