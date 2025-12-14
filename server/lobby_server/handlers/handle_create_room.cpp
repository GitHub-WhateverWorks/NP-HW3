#include "../lobby_server.hpp"
#include "../../database/db.hpp"

void handleCreateRoom(TCPConnection &conn, const nlohmann::json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "CREATE_ROOM";

    if (!d.contains("game_id") || !d.contains("player_id")) {
        r.data["ok"]  = false;
        r.data["msg"] = "Missing game_id/player_id.";
        conn.sendPacket(r);
        return;
    }

    int gid = d["game_id"];
    int pid = d["player_id"];

    auto *server = reinterpret_cast<LobbyServer*>(conn.owner);
    if (!server) {
        r.data["ok"]  = false;
        r.data["msg"] = "Internal server error.";
        conn.sendPacket(r);
        return;
    }

    // read maxPlayers once from DB
    int maxPlayers = 2;
    auto games = Database::instance().listActiveGames();
    for (auto &g : games) {
        if (g.id == gid) {
            maxPlayers = g.maxPlayers;
            break;
        }
    }

    int rid = server->createRoom(gid, pid, maxPlayers);
    Room *room = server->getRoom(rid);

    r.data["ok"]      = true;
    r.data["room_id"] = rid;
    r.data["game_id"] = gid;
    r.data["players"] = room->players;

    conn.sendPacket(r);
}
