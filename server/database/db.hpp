#ifndef DB_HPP
#define DB_HPP

#include "../shared/json.hpp"
#include <mutex>
#include <string>
#include <vector>

using nlohmann::json;

struct GameInfo {
    int         id = 0;
    std::string name;
    std::string authorName;
    std::string description;
    std::string gameType;
    int         maxPlayers = 0;
    bool        isActive   = false;
    std::string latestVersion;
};


struct GameRecord {
    int         id = 0;
    int         developerId = 0;       
    std::string name;
    std::string description;
    std::string gameType;
    int         maxPlayers = 0;
    bool        active = true;

    json versions;
};

class Database {
public:
    static Database &instance();

    bool load(const std::string &filename);
    bool save();

    //Developer accounts
    int createDeveloper(const std::string &username,
                        const std::string &password_plain);
    int authenticateDeveloper(const std::string &username,
                              const std::string &password_plain);

    //Player accounts
    int createPlayer(const std::string &username,
                     const std::string &password_plain);
    int authenticatePlayer(const std::string &username,
                           const std::string &password_plain);

    //Game Management
    int createGame(int developerId,
                   const std::string &name,
                   const std::string &description,
                   const std::string &gameType,
                   int maxPlayers);

    int addGameVersion(int gameId,
                       const std::string &versionStr,
                       const std::string &storagePath);

    //Soft delete
    bool deactivateGame(int gameId, int developerId);

    //Ownership check
    bool isGameOwnedBy(int gameId, int developerId);

    //Dev panel
    std::vector<GameRecord> listDeveloperGames(int developerId);

    //Lobby list
    std::vector<GameInfo> listActiveGames();

    //Download helpers
    std::string getLatestVersionString(int gameId);
    std::string getLatestVersionStoragePath(int gameId);
    bool addReview(int gameId, int playerId, int score, const std::string &comment);
    json getGameReviews(int gameId);
    void init();

private:
    Database();

    json m_root;
    std::mutex m_mutex;
    std::string m_filename;

    void initIfEmpty();
    void ensureCounters();

    int nextId(const std::string &counter);
    int findDeveloperId(const std::string &username);
    int findPlayerId(const std::string &username);

    json *findGameJson(int gameId);    
    GameRecord jsonToRecord(const json &g); 
    bool saveUnlocked();
};

#endif
