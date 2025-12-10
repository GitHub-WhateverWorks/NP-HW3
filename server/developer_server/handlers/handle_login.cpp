#include "../developer_server.hpp"
#include "../../database/db.hpp"
using json = nlohmann::json;

void handleDeveloperLogin(TCPConnection &conn, const json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "DEV_LOGIN";

    if (!d.contains("username") || !d.contains("password")) {
        r.data["ok"] = false;
        r.data["msg"] = "Missing username/password.";
        conn.sendPacket(r);
        return;
    }

    int id = Database::instance().authenticateDeveloper(
        d["username"], d["password"]);

    if (id < 0) {
        r.data["ok"] = false;
        r.data["msg"] = "Invalid credentials.";
    } else {
        r.data["ok"] = true;
        r.data["dev_id"] = id;
    }

    conn.sendPacket(r);
}
