#include "db.hpp"

#include <fstream>
#include <iostream>
#include <functional>
#include <ctime>

using nlohmann::json;

static std::string hashPassword(const std::string &password_plain) {
    std::hash<std::string> h;
    return std::to_string(h(password_plain));
}

Database &Database::instance() {
    static Database inst;
    return inst;
}

Database::Database()
    : m_root(json::object()),
      m_filename("tables.json") {
}


bool Database::load(const std::string &filename) {
    std::lock_guard<std::mutex> guard(m_mutex);

    m_filename = std::filesystem::absolute(filename).string();
    std::cout << "[DB] Loading DB from: " << m_filename << "\n";

    std::ifstream in(m_filename);
    if (!in.is_open()) {
        std::cout << "[DB] File not found — creating new DB\n";
        m_root = json::object();
        initIfEmpty();
        save();
        return true;
    }

    in.seekg(0, std::ios::end);
    std::streampos size = in.tellg();
    in.seekg(0, std::ios::beg);

    //std::cout << "[DB] Existing file size: " << size << " bytes\n";

    if (size == 0) {
        std::cout << "[DB] File empty — reinitializing\n";
        m_root = json::object();
        initIfEmpty();
        save();
        return true;
    }

    try {
        in >> m_root;
        //std::cout << "[DB] Parsed JSON OK\n";
    } catch (const std::exception &e) {
        std::cout << "[DB] Parse failed: " << e.what() << "\n";
        m_root = json::object();
        initIfEmpty();
        save();
        return false;
    }

    initIfEmpty();
    /*
    std::cout << "[DB] After initIfEmpty(), DB state:\n"
              << m_root.dump(4) << "\n";
    */
    return true;
}

bool Database::saveUnlocked() {
    std::ofstream out(m_filename);
    if (!out.is_open()) {
        std::cerr << "[DB] Failed to open '" << m_filename
                  << "' for writing\n";
        return false;
    }
    out << m_root.dump(4);
    return true;
}
bool Database::save() {
    //std::lock_guard<std::mutex> guard(m_mutex);

    if (m_filename.empty()) {
        m_filename = std::filesystem::absolute("tables.json").string();
    }

    std::cout << "[DB] Saving DB to: " << m_filename << "\n";
    std::cout << "[DB] JSON to write:\n" << m_root.dump(4) << "\n";

    std::ofstream out(m_filename, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "[DB] Failed to open '" << m_filename
                  << "' for writing\n";
        return false;
    }

    out << m_root.dump(4);
    out.flush();

    if (!out.good()) {
        std::cerr << "[DB] Write error when saving '" << m_filename << "'\n";
        return false;
    }

    std::cout << "[DB] Save complete.\n";
    return true;
}


void Database::initIfEmpty() {
    //std::cout<<"[DB] initIfEmpty\n";
    if (!m_root.is_object()) {
        m_root = json::object();
    }

    if (!m_root.contains("developers")) m_root["developers"] = json::array();
    if (!m_root.contains("players"))    m_root["players"]    = json::array();
    if (!m_root.contains("games"))      m_root["games"]      = json::array();
    if (!m_root.contains("versions"))   m_root["versions"]   = json::array();
    if (!m_root.contains("reviews"))    m_root["reviews"]    = json::array();
    if (!m_root.contains("rooms"))      m_root["rooms"]      = json::array();

    ensureCounters();
}

void Database::ensureCounters() {
    //std::cout<<"[DB] ensureCounters\n";
    if (!m_root.contains("counters") || !m_root["counters"].is_object()) {
        m_root["counters"] = json::object();
    }

    auto &c = m_root["counters"];
    if (!c.contains("developer_id")) c["developer_id"] = 1;
    if (!c.contains("player_id"))    c["player_id"]    = 1;
    if (!c.contains("game_id"))      c["game_id"]      = 1;
    if (!c.contains("version_id"))   c["version_id"]   = 1;
    if (!c.contains("review_id"))    c["review_id"]    = 1;
    if (!c.contains("room_id"))      c["room_id"]      = 1;
}

int Database::nextId(const std::string &counter) {
    auto &c = m_root["counters"];
    int id = 1;
    if (c.contains(counter)) {
        id = c[counter].get<int>();
    }
    c[counter] = id + 1;
    return id;
}


int Database::findDeveloperId(const std::string &username) {
    auto &devs = m_root["developers"];
    for (const auto &d : devs) {
        if (d.value("username", std::string()) == username) {
            return d.value("id", -1);
        }
    }
    return -1;
}

int Database::findPlayerId(const std::string &username) {
    auto &players = m_root["players"];
    for (const auto &p : players) {
        if (p.value("username", std::string()) == username) {
            return p.value("id", -1);
        }
    }
    return -1;
}

json *Database::findGameJson(int gameId) {
    auto &games = m_root["games"];
    for (auto &g : games) {
        if (g.value("id", -1) == gameId) {
            return &g;
        }
    }
    return nullptr;
}

GameRecord Database::jsonToRecord(const json &g) {
    GameRecord rec;

    rec.id          = g.value("id", 0);
    rec.developerId = g.value("author_dev_id",
                        g.value("developer_id", 0));
    rec.name        = g.value("name", std::string());
    rec.description = g.value("description", std::string());
    rec.gameType    = g.value("game_type",
                        g.value("gameType", std::string()));
    rec.maxPlayers  = g.value("max_players",
                        g.value("maxPlayers", 0));
    rec.active      = g.value("is_active",
                        g.value("active", true));

    rec.versions = json::object();
    if (m_root.contains("versions") && m_root["versions"].is_array()) {
        for (const auto &v : m_root["versions"]) {
            if (v.value("game_id", 0) == rec.id) {
                std::string vstr  = v.value("version_str", std::string());
                std::string spath = v.value("storage_path", std::string());
                if (!vstr.empty()) {
                    rec.versions[vstr] = spath;
                }
            }
        }
    }

    return rec;
}


int Database::createDeveloper(const std::string &username,
                              const std::string &password_plain) {
    std::cout << "[DEBUG][DB] createDeveloper ENTER\n";
    std::lock_guard<std::mutex> guard(m_mutex);
    std::cout << "[DEBUG][DB] createDeveloper LOCK ACQUIRED\n";
    if (findDeveloperId(username) != -1) {
        return -1;
    }

    int id = nextId("developer_id");

    json dev = {
        {"id", id},
        {"username", username},
        {"password_hash", hashPassword(password_plain)}
    };

    m_root["developers"].push_back(dev);
    save();
    return id;
}

int Database::authenticateDeveloper(const std::string &username,
                                    const std::string &password_plain) {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::string hash = hashPassword(password_plain);
    auto &devs = m_root["developers"];

    for (const auto &d : devs) {
        if (d.value("username", std::string()) == username &&
            d.value("password_hash", std::string()) == hash) {
            return d.value("id", -1);
        }
    }
    return -1;
}

int Database::createPlayer(const std::string &username,
                           const std::string &password_plain) {
    std::lock_guard<std::mutex> guard(m_mutex);

    if (findPlayerId(username) != -1) {
        return -1;
    }

    int id = nextId("player_id");

    json player = {
        {"id", id},
        {"username", username},
        {"password_hash", hashPassword(password_plain)}
    };

    m_root["players"].push_back(player);
    save();
    return id;
}

int Database::authenticatePlayer(const std::string &username,
                                 const std::string &password_plain) {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::string hash = hashPassword(password_plain);
    auto &players = m_root["players"];

    for (const auto &p : players) {
        if (p.value("username", std::string()) == username &&
            p.value("password_hash", std::string()) == hash) {
            return p.value("id", -1);
        }
    }
    return -1;
}


int Database::createGame(int developerId,
                         const std::string &name,
                         const std::string &description,
                         const std::string &gameType,
                         int maxPlayers) {
    std::lock_guard<std::mutex> guard(m_mutex);

    auto &games = m_root["games"];

    for (const auto &g : games) {
        if (g.value("author_dev_id", 0) == developerId &&
            g.value("name", std::string()) == name &&
            g.value("is_active", true)) {
            return -1; 
        }
    }

    int id = nextId("game_id");

    json game = {
        {"id", id},
        {"author_dev_id", developerId},
        {"name", name},
        {"description", description},
        {"game_type", gameType},
        {"max_players", maxPlayers},
        {"is_active", true},
        {"latest_version_id", nullptr} 
    };

    games.push_back(game);
    save();
    return id;
}

int Database::addGameVersion(int gameId,
                             const std::string &versionStr,
                             const std::string &storagePath) {
    std::lock_guard<std::mutex> guard(m_mutex);

    json *game = findGameJson(gameId);
    if (!game) {
        return -1;
    }

    int vid = nextId("version_id");
    std::time_t t = std::time(nullptr);

    json ver = {
        {"id", vid},
        {"game_id", gameId},
        {"version_str", versionStr},
        {"storage_path", storagePath},
        {"created_at", std::to_string(t)}
    };

    m_root["versions"].push_back(ver);

    (*game)["latest_version_id"] = vid;

    save();
    return vid;
}

bool Database::deactivateGame(int gameId, int developerId) {
    std::lock_guard<std::mutex> guard(m_mutex);

    json *game = findGameJson(gameId);
    if (!game) return false;

    int author = game->value("author_dev_id", 0);
    if (author != developerId) {
        return false; 
    }

    (*game)["is_active"] = false;
    save();
    return true;
}

bool Database::isGameOwnedBy(int gameId, int developerId) {
    std::lock_guard<std::mutex> guard(m_mutex);

    json *game = findGameJson(gameId);
    if (!game) return false;

    int author = game->value("author_dev_id", 0);
    return (author == developerId);
}

std::vector<GameRecord> Database::listDeveloperGames(int developerId) {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::vector<GameRecord> out;
    auto &games = m_root["games"];

    for (const auto &g : games) {
        int owner = g.value("author_dev_id", 0);
        if (owner == developerId) {
            out.push_back(jsonToRecord(g));
        }
    }
    return out;
}


std::vector<GameInfo> Database::listActiveGames() {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::vector<GameInfo> out;

    auto &games    = m_root["games"];
    auto &devs     = m_root["developers"];
    auto &versions = m_root["versions"];

    auto getDevName = [&](int devId) -> std::string {
        for (const auto &d : devs) {
            if (d.value("id", -1) == devId) {
                return d.value("username", std::string("<unknown>"));
            }
        }
        return "<unknown>";
    };

    auto getVersionStrById = [&](int vid) -> std::string {
        for (const auto &v : versions) {
            if (v.value("id", -1) == vid) {
                return v.value("version_str", std::string());
            }
        }
        return "";
    };

    for (const auto &g : games) {
        if (!g.value("is_active", true)) {
            continue;
        }

        GameInfo info;
        info.id          = g.value("id", 0);
        info.name        = g.value("name", std::string());
        info.authorName  = getDevName(g.value("author_dev_id", 0));
        info.description = g.value("description", std::string());
        info.gameType    = g.value("game_type", std::string());
        info.maxPlayers  = g.value("max_players", 0);
        info.isActive    = true;

        if (!g["latest_version_id"].is_null()) {
            int vid = g["latest_version_id"].get<int>();
            info.latestVersion = getVersionStrById(vid);
        } else {
            info.latestVersion = "";
        }

        out.push_back(info);
    }

    return out;
}


std::string Database::getLatestVersionString(int gameId) {
    std::lock_guard<std::mutex> guard(m_mutex);

    json *g = findGameJson(gameId);
    if (!g || (*g)["latest_version_id"].is_null()) {
        return "";
    }

    int vid = (*g)["latest_version_id"].get<int>();
    auto &vers = m_root["versions"];

    for (const auto &v : vers) {
        if (v.value("id", -1) == vid) {
            return v.value("version_str", std::string());
        }
    }
    return "";
}

std::string Database::getLatestVersionStoragePath(int gameId) {
    std::lock_guard<std::mutex> guard(m_mutex);

    json *g = findGameJson(gameId);
    if (!g || (*g)["latest_version_id"].is_null()) {
        return "";
    }

    int vid = (*g)["latest_version_id"].get<int>();
    auto &vers = m_root["versions"];

    for (const auto &v : vers) {
        if (v.value("id", -1) == vid) {
            return v.value("storage_path", std::string());
        }
    }
    return "";
}

bool Database::addReview(int gameId, int playerId, int score, const std::string &comment) {
    std::lock_guard<std::mutex> guard(m_mutex);

    if (score < 1 || score > 5) return false;

    int id = nextId("review_id");

    json r = {
        {"id", id},
        {"game_id", gameId},
        {"player_id", playerId},
        {"score", score},
        {"comment", comment}
    };

    m_root["reviews"].push_back(r);
    save();
    return true;
}

json Database::getGameReviews(int gameId) {
    json out = json::array();
    for (auto &r : m_root["reviews"]) {
        if (r.value("game_id", -1) == gameId) {
            out.push_back(r);
        }
    }
    return out;
}
