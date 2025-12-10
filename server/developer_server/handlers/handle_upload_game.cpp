#include "../developer_server.hpp"
#include "../../database/db.hpp"
#include "../base64.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace fs = std::filesystem;

static bool writeBinary(const std::string &p, const std::vector<uint8_t>&buf) {
    std::ofstream f(p, std::ios::binary);
    if (!f) return false;
    f.write((char*)buf.data(), buf.size());
    return true;
}

void handleUploadGame(TCPConnection &conn, const json &d) {

    std::cout << "[DEBUG][SERVER] handleUploadGame incoming: "
              << d.dump() << "\n";

    Packet r;
    r.type = PacketType::SERVER_RESPONSE;
    r.data["kind"] = "DEV_UPLOAD_GAME";

    if (!d.contains("dev_id") ||
        !d.contains("version_str") ||
        !d.contains("filename") ||
        !d.contains("filedata_base64"))
    {
        r.data["ok"] = false;
        r.data["msg"] = "Missing shared required fields";
        conn.sendPacket(r);
        return;
    }

    int devId      = d.value("dev_id", -1);
    std::string ver= d.value("version_str", "");
    std::string fn = d.value("filename", "");
    std::string b64= d.value("filedata_base64", "");

    bool isUpdate = d.contains("game_id");

    int gameId = -1;

    if (isUpdate) {

        if (!d.contains("game_id")) {
            r.data["ok"] = false;
            r.data["msg"] = "Missing game_id for update";
            conn.sendPacket(r);
            return;
        }

        gameId = d.value("game_id", -1);

        if (gameId <= 0) {
            r.data["ok"] = false;
            r.data["msg"] = "Invalid game_id";
            conn.sendPacket(r);
            return;
        }

        // Ownership check
        if (!Database::instance().isGameOwnedBy(gameId, devId)) {
            r.data["ok"] = false;
            r.data["msg"] = "You do not own this game";
            conn.sendPacket(r);
            return;
        }
    }

    else {
        if (!d.contains("game_name") ||
            !d.contains("description") ||
            !d.contains("game_type") ||
            !d.contains("max_players"))
        {
            r.data["ok"] = false;
            r.data["msg"] = "Missing new-game fields";
            conn.sendPacket(r);
            return;
        }

        std::string gname = d.value("game_name", "");
        std::string desc  = d.value("description", "");
        std::string gtype = d.value("game_type", "");
        int maxP          = d.value("max_players", -1);

        if (gname.empty() || desc.empty() || gtype.empty() || maxP <= 0) {
            r.data["ok"] = false;
            r.data["msg"] = "Invalid new-game fields";
            conn.sendPacket(r);
            return;
        }

        gameId = Database::instance().createGame(
            devId, gname, desc, gtype, maxP
        );

        if (gameId <= 0) {
            r.data["ok"] = false;
            r.data["msg"] = "Failed to create new game (duplicate name?)";
            conn.sendPacket(r);
            return;
        }

        std::cout << "[DEBUG][SERVER] Created new game_id=" << gameId << "\n";
    }

    std::vector<uint8_t> rawZip = decodeBase64(b64);
    if (rawZip.empty()) {
        r.data["ok"] = false;
        r.data["msg"] = "Empty or invalid base64 ZIP";
        conn.sendPacket(r);
        return;
    }

    std::string verFolder =
        "uploaded_games/game_" + std::to_string(gameId) + "/" + ver + "/";

    fs::create_directories(verFolder);

    std::string zipPath = verFolder + fn;

    if (!writeBinary(zipPath, rawZip)) {
        r.data["ok"] = false;
        r.data["msg"] = "Failed to write ZIP file";
        conn.sendPacket(r);
        return;
    }

    // Unzip
    std::cout<<"unzipping\n";
    std::string cmd = "unzip -o \"" + zipPath + "\" -d \"" + verFolder + "\"";
    system(cmd.c_str());
    std::cout<<"unzipping complete\n";
    // After unzip, auto-chmod everything inside /server and /client_* folders
    {
    std::vector<std::string> dirs = {
        verFolder + "server",
        verFolder + "client_cli",
        verFolder + "client_gui"
    };

    for (auto &dir : dirs) {
        if (!fs::exists(dir)) continue;

        for (auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string p = entry.path().string();
                ::chmod(p.c_str(), 0755);
                std::cout << "[DEBUG] chmod +x " << p << "\n";
            }
        }
    }
}

    int verId = Database::instance().addGameVersion(gameId, ver, verFolder);

    r.data["ok"]          = true;
    r.data["game_id"]     = gameId;
    r.data["version_id"]  = verId;
    r.data["version_str"] = ver;

    conn.sendPacket(r);
}

void handleUpdateGame(TCPConnection &conn, const nlohmann::json &d) {
    handleUploadGame(conn, d);
}