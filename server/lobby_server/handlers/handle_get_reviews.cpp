#include "../lobby_server.hpp"
#include "../../database/db.hpp"

using json = nlohmann::json;

void handleGetReviews(TCPConnection &conn, const json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "PLAYER_REVIEWS";

    if (!d.contains("game_id")) {
        r.data["ok"]  = false;
        r.data["msg"] = "Missing game_id.";
        conn.sendPacket(r);
        return;
    }

    int gameId = d["game_id"];

    auto reviews = Database::instance().getGameReviews(gameId);

    r.data["ok"]      = true;
    r.data["reviews"] = reviews;

    conn.sendPacket(r);
}
