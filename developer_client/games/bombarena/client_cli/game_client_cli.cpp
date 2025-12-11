#include <iostream>
#include <thread>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "../shared/tcp.hpp"
#include "../shared/packet.hpp"
#include "../engine/engine.hpp"

using namespace bombarena;


static void setNonBlockingInput() {
    termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

static char readKey() {
    char c;
    if (read(STDIN_FILENO, &c, 1) > 0)
        return c;
    return 0;
}


class BombArenaClientCLI {
public:
    BombArenaClientCLI(const std::string &ip, int port)
        : running(true), gameStarted(false), playerId(-1)
    {
        if (!conn.connectToServer(ip, port)) {
            std::cerr << "[CLI] Cannot connect to game server\n";
            std::exit(1);
        }

        std::cout << "[CLI] Connected. Waiting for JOIN_GAME...\n";

        localState = initTwoPlayerDefault();
        localState.players.clear();
        localState.bombs.clear();
        localState.lastExplosionCells.clear();
        localState.turnNumber = 0;
    }

    void start() {
        setNonBlockingInput();
        std::thread net(&BombArenaClientCLI::networkThread, this);
        mainLoop();
        running = false;
        net.join();
    }

private:
    TCPConnection conn;
    std::atomic<bool> running;
    std::atomic<bool> gameStarted;
    int playerId;

    GameState localState;

    void mainLoop() {
        redrawWaiting();

        while (running) {
            char c = readKey();

            if (c != 0) {
                if (!gameStarted) {

                    if (playerId == 1 && (c == 'g' || c == 'G')) {
                        Packet p;
                        p.type = PacketType::PLAYER_START_GAME;
                        conn.sendPacket(p);
                        std::cout << "[CLI] Sent start request.\n";
                    }
                } else {

                    char action = 0;

                    if (c == 'w' || c == 'W') action = 'w';
                    else if (c == 's' || c == 'S') action = 's';
                    else if (c == 'a' || c == 'A') action = 'a';
                    else if (c == 'd' || c == 'D') action = 'd';
                    else if (c == 'b' || c == 'B') action = 'b';
                    else if (c == 'x' || c == 'X') action = 'x'; 

                    if (action != 0) {
                        Packet p;
                        p.type = PacketType::PLAYER_ACTION;
                        p.data["action"] = std::string(1, action);
                        conn.sendPacket(p);
                    }
                }
            }

            usleep(20000);
        }
    }


    void networkThread() {
        while (running) {
            Packet p;
            if (!conn.recvPacket(p)) {
                std::cerr << "[CLI] Disconnected from server.\n";
                running = false;
                break;
            }

            switch (p.type) {
                case PacketType::JOIN_GAME:
                    playerId = p.data["player_id"];
                    std::cout << "[CLI] You are Player " << playerId << "\n";
                    redrawWaiting();
                    break;

                case PacketType::PLAYER_START_GAME:
                    std::cout << "\n[CLI] GAME STARTED\n";
                    gameStarted = true;
                    break;

                case PacketType::STATE_UPDATE:
                    updateLocalState(p.data);
                    redraw();
                    break;

                case PacketType::GAME_END:
                    running = false;
                    showGameResult(p.data);
                    break;

                default:
                    break;
            }
        }
    }


    void updateLocalState(const nlohmann::json &d) {
        localState.turnNumber = d["turn"];

        localState.players.clear();
        for (auto &p : d["players"]) {
            PlayerState ps;
            ps.id    = p["id"];
            ps.x     = p["x"];
            ps.y     = p["y"];
            ps.alive = p["alive"];
            localState.players.push_back(ps);
        }

        localState.bombs.clear();
        for (auto &b : d["bombs"]) {
            Bomb bb;
            bb.x     = b["x"];
            bb.y     = b["y"];
            bb.timer = b["timer"];
            if (b.contains("ownerId")) bb.ownerId = b["ownerId"];
            if (b.contains("range"))   bb.range   = b["range"];
            localState.bombs.push_back(bb);
        }

        localState.lastExplosionCells.clear();
        if (d.contains("explosions")) {
            for (auto &e : d["explosions"])
                localState.lastExplosionCells.emplace_back(e["x"], e["y"]);
        }
    }


    void redrawWaiting() {
        system("clear");
        std::cout << "=== BombArena CLI ===\n";

        if (playerId == -1) {
            std::cout << "Waiting for JOIN_GAME...\n";
        } else {
            std::cout << "You are Player " << playerId << "\n";
            std::cout << "Waiting for host to start.\n";

            if (playerId == 1)
                std::cout << "Press 'g' to start the game.\n";
        }
        std::cout << "\n";
    }


    void redraw() {
        system("clear");
        std::cout << "=== BombArena CLI ===\n";
        std::cout << "Player " << playerId
                  << " | Turn " << localState.turnNumber << "\n";
        std::cout << "(w/a/s/d = move, b = bomb, x = stay)\n\n";


        std::cout << renderBoard(localState) << "\n";


        if (!localState.lastExplosionCells.empty()) {
            std::cout << "Explosions at: ";
            for (auto &c : localState.lastExplosionCells)
                std::cout << "(" << c.first << "," << c.second << ") ";
            std::cout << "\n";
        }
    }


    void showGameResult(const nlohmann::json &d) {
        std::cout << "\n=== GAME OVER ===\n";

        if (d["result"] == "draw") {
            std::cout << "DRAW!\n";
            return;
        }

        int w = d["winner"];
        if (w == playerId)
            std::cout << "YOU WIN!\n";
        else
            std::cout << "You lose. Winner = Player " << w << "\n";
    }
};


int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Usage: bombarena_client_cli <ip> <port>\n";
        return 1;
    }

    BombArenaClientCLI cli(argv[1], std::stoi(argv[2]));
    cli.start();
    return 0;
}
