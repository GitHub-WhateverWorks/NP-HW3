#include "../lobby_server.hpp"
#include "../../database/db.hpp"

using json = nlohmann::json;

void handleSubmitReview(TCPConnection &conn, const json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "PLAYER_REVIEW_RESULT";

    if (!d.contains("player_id") ||
        !d.contains("game_id")   ||
        !d.contains("score")     ||
        !d.contains("comment")) {

        r.data["ok"]  = false;
        r.data["msg"] = "Missing review fields.";
        conn.sendPacket(r);
        return;
    }

    int playerId = d["player_id"];
    int gameId   = d["game_id"];
    int score    = d["score"];

    std::string comment = d["comment"];

    if (score < 1 || score > 5) {
        r.data["ok"]  = false;
        r.data["msg"] = "Score must be between 1 and 5.";
        conn.sendPacket(r);
        return;
    }

    bool ok = Database::instance().addReview(gameId, playerId, score, comment);

    r.data["ok"] = ok;
    if (!ok)
        r.data["msg"] = "Database insert failed.";

    conn.sendPacket(r);
}
