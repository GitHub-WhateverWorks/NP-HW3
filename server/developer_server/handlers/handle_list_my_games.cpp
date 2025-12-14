#include "../developer_server.hpp"
#include "../../database/db.hpp"

void handleListMyGames(TCPConnection &conn, const json &data) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;

    if (!data.contains("dev_id")) {
        r.data["ok"] = false;
        r.data["kind"] = "DEV_LIST_MY_GAMES";
        r.data["msg"] = "Missing dev_id";
        conn.sendPacket(r);
        return;
    }

    int devId = data["dev_id"].get<int>();

    auto list = Database::instance().listDeveloperGames(devId);

    r.data["ok"] = true;
    r.data["kind"] = "DEV_LIST_MY_GAMES";
    r.data["games"] = json::array();

    for (const auto &g : list) {
        json obj;

        obj["game_id"]     = g.id;
        obj["name"]        = g.name;
        obj["description"] = g.description;
        obj["game_type"]   = g.gameType;
        obj["max_players"] = g.maxPlayers;
        obj["active"]      = g.active;

        std::string latest = "";
        for (auto it = g.versions.begin(); it != g.versions.end(); ++it) {
            latest = it.key(); 
        }
        obj["latest_version"] = latest;

        r.data["games"].push_back(obj);
    }

    conn.sendPacket(r);
}
