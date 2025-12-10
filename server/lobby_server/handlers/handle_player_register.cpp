#include "../lobby_server.hpp"
#include "../../database/db.hpp"

void handlePlayerRegister(TCPConnection &conn, const nlohmann::json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "PLAYER_REGISTER";

    if (!d.contains("username") || !d.contains("password")) {
        r.data["ok"] = false;
        r.data["msg"] = "Missing register fields.";
        conn.sendPacket(r);
        return;
    }

    int pid = Database::instance().createPlayer(
        d["username"], d["password"]
    );

    if (pid < 0) {
        r.data["ok"] = false;
        r.data["msg"] = "Registration failed (username exists?).";
    } else {
        r.data["ok"] = true;
        r.data["player_id"] = pid;
    }

    conn.sendPacket(r);
}
