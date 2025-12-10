#include "db.hpp"
#include <iostream>

int main() {
    std::cout<< "test_db start\n";
    auto &db = Database::instance();
    std::cout<<"instance complete\n";
    if (!db.load("tables.json")) {
        std::cerr << "Failed to load DB\n";
        return 1;
    }

    int devId = db.createDeveloper("alice_dev", "password123");
    std::cout << "devId = " << devId << "\n";

    int auth = db.authenticateDeveloper("alice_dev", "password123");
    std::cout << "auth dev = " << auth << "\n";

    int gameId = db.createGame(devId, "TicTacToe", "Simple 2P game", "CLI", 2);
    std::cout << "gameId = " << gameId << "\n";

    int verId = db.addGameVersion(gameId, "1.0.0", "uploaded/g1_v1.zip");
    std::cout << "verId = " << verId << "\n";

    auto games = db.listActiveGames();
    for (auto &g : games) {
        std::cout << "Game: " << g.name << " by " << g.authorName
                  << " latest=" << g.latestVersion << "\n";
    }

    return 0;
}
