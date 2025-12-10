#include "../lobby_server.hpp"
#include "../../database/db.hpp"

void handlePlayerLogin(TCPConnection &conn, const nlohmann::json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "PLAYER_LOGIN";

    if (!d.contains("username") || !d.contains("password")) {
        r.data["ok"]  = false;
        r.data["msg"] = "Missing login fields.";
        conn.sendPacket(r);
        return;
    }

    const std::string username = d["username"].get<std::string>();
    const std::string password = d["password"].get<std::string>();

    int pid = Database::instance().authenticatePlayer(username, password);

    auto *server = reinterpret_cast<LobbyServer*>(conn.owner);

    if (pid < 0) {
        r.data["ok"]  = false;
        r.data["msg"] = "Invalid credentials.";
    }
    else if (server->isPlayerOnline(pid)) {
        r.data["ok"]  = false;
        r.data["msg"] = "This account is already logged in from another client.";
    }
    else {
        // First login for this player → register the mapping
        int fd = conn.fd();
        server->registerPlayer(pid, fd);

        r.data["ok"]       = true;
        r.data["player_id"] = pid;
    }

    conn.sendPacket(r);
}
