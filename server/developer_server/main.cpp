#include "developer_server.hpp"
#include "../database/db.hpp"


int main(int argc, char **argv) {
    int port = 15000;
    if (argc >= 2)
        port = std::stoi(argv[1]);

    Database::instance().load("tables.json");

    DeveloperServer server(port);
    if (!server.start()) {
        std::cerr << "[DeveloperServer] Failed to start on port "
                  << port << "\n";
        return 1;
    }

    std::cout << "[DeveloperServer] Running on port " << port << "\n";

    while (true) {
        sleep(1);
    }
}
