#include "../lobby_server.hpp"
#include "../../database/db.hpp"

void handleListGames(TCPConnection &conn, const nlohmann::json &) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "PLAYER_LIST_GAMES";

    auto games = Database::instance().listActiveGames();
    r.data["ok"] = true;

    r.data["games"] = nlohmann::json::array();

    for (auto &g : games) {
        nlohmann::json e;
        e["game_id"]       = g.id;
        e["name"]          = g.name;
        e["author"]        = g.authorName;
        e["description"]   = g.description;
        e["game_type"]     = g.gameType;
        e["max_players"]   = g.maxPlayers;
        std::string latestVersionStr = "";

        e["latest_version"] = Database::instance().getLatestVersionString(g.id);

        r.data["games"].push_back(e);
    }

    conn.sendPacket(r);
}
