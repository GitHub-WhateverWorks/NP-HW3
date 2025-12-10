#include "game_server.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::cout<<"starting game_server...\n";
    int port = 16000; // fallback only 

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            try {
                port = std::stoi(argv[i + 1]);
            } catch (...) {
                std::cerr << "[BombArenaServer] Invalid port: "
                          << argv[i + 1] << "\n";
                return 1;
            }
            i++; 
        }
    }

    if (port < 10000) {
        std::cerr << "[BombArenaServer] ERROR: CSIT requires port >= 10000\n";
        return 1;
    }

    std::cout << "[BombArenaServer] Starting on port " << port << "...\n";

    BombArenaServer server(port);
    server.run();

    return 0;
}
