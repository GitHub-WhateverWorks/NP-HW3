#include "../lobby_server.hpp"

static int parseRoomId(const nlohmann::json &v) {
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_number_unsigned()) return (int)v.get<unsigned>();
    if (v.is_string()) {
        try { return std::stoi(v.get<std::string>()); }
        catch (...) { return -1; }
    }
    return -1;
}

void handleJoinRoom(TCPConnection &conn, const json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "JOIN_ROOM";

    if (!d.contains("room_id") || !d.contains("player_id")) {
        r.data["ok"] = false;
        r.data["msg"] = "Missing room_id/player_id.";
        conn.sendPacket(r);
        return;
    }

    if (!d["room_id"].is_number()) {
        r.data["ok"] = false;
        r.data["msg"] = "room_id must be a number.";
        conn.sendPacket(r);
        return;
    }

    int rid = d["room_id"].get<int>();
    int pid = d["player_id"].get<int>();

    auto *server = reinterpret_cast<LobbyServer*>(conn.owner);
    Room *room = server->getRoom(rid);

    if (!room) {
        r.data["ok"] = false;
        r.data["msg"] = "Room not found.";
        conn.sendPacket(r);
        return;
    }

    if ((int)room->players.size() >= room->maxPlayers) {
        r.data["ok"] = false;
        r.data["msg"] = "Room full.";
        conn.sendPacket(r);
        return;
    }

    // Prevent duplicate join
    bool alreadyIn = false;
    for (int p : room->players) {
        if (p == pid) {
            alreadyIn = true;
            break;
        }
    }
    if (!alreadyIn) {
        room->players.push_back(pid);
    }

    // Reply only to the joining player
    r.data["ok"]      = true;
    r.data["room_id"] = rid;
    r.data["game_id"] = room->gameId;
    r.data["players"] = room->players;
    conn.sendPacket(r);
}
