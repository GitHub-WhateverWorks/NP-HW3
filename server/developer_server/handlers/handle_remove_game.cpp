#include "../developer_server.hpp"
#include "../../database/db.hpp"

using json = nlohmann::json;

void handleRemoveGame(TCPConnection &conn, const json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "DEV_REMOVE_GAME";

    if (!d.contains("dev_id") || !d.contains("game_id")) {
        r.data["ok"] = false;
        r.data["msg"] = "Missing dev_id/game_id";
        conn.sendPacket(r);
        return;
    }

    int dev = d["dev_id"];
    int gid = d["game_id"];

    if (!Database::instance().isGameOwnedBy(gid, dev)) {
        r.data["ok"] = false;
        r.data["msg"] = "Not your game.";
        conn.sendPacket(r);
        return;
    }

    if (!Database::instance().deactivateGame(gid, dev)) {
        r.data["ok"] = false;
        r.data["msg"] = "Failed to deactivate game.";
        conn.sendPacket(r);
        return;
    }

    r.data["ok"] = true;
    conn.sendPacket(r);
}
