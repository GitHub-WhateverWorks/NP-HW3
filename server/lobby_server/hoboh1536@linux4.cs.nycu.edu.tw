#include "lobby_server.hpp"
#include "../database/db.hpp"
#include <iostream>
#include <unistd.h> 
#include <signal.h>
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    int port = 17000;
    if (argc >= 2)
        port = std::stoi(argv[1]);

    if (!Database::instance().load("tables.json")) {
        std::cerr << "[LobbyServer] Failed to load database.\n";
        return 1;
    }

    LobbyServer server(port);
    if (!server.start()) {
        std::cerr << "[LobbyServer] Failed to start on port " << port << "\n";
        return 1;
    }

    std::cout << "[LobbyServer] Running on port " << port << "\n";

    while (true) {
        Database::instance().load("tables.json")
        sleep(1);
    }

    return 0;
}
