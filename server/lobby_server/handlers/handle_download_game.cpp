#include "../lobby_server.hpp"
#include "../../database/db.hpp"
#include "../../developer_server/base64.hpp"

#include <fstream>
#include <vector>

void handleDownloadGame(TCPConnection &conn, const nlohmann::json &d) {
    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "GAME_DOWNLOAD";

    if (!d.contains("game_id")) {
        r.data["ok"] = false;
        r.data["msg"] = "Missing game_id." ;
        conn.sendPacket(r);
        return;
    }

    int gid = d["game_id"];

    std::string ver = Database::instance().getLatestVersionString(gid);
    std::string folder = Database::instance().getLatestVersionStoragePath(gid);

    if (ver.empty() || folder.empty()) {
        r.data["ok"] = false;
        r.data["msg"] = "Game not found.";
        conn.sendPacket(r);
        return;
    }

    std::string zipFile = folder + "game.zip";

    std::ifstream fin(zipFile, std::ios::binary);
    if (!fin) {
        r.data["ok"] = false;
        r.data["msg"] = "Missing game.zip on server.";
        conn.sendPacket(r);
        return;
    }

    std::vector<uint8_t> raw(
        (std::istreambuf_iterator<char>(fin)),
        std::istreambuf_iterator<char>()
    );

    std::string b64 = encodeBase64(raw);

    r.data["ok"] = true;
    r.data["version"] = ver;
    r.data["filename"] = "game.zip";
    r.data["filedata_base64"] = b64;

    conn.sendPacket(r);
}
