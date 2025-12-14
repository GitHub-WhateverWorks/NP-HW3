#include <SFML/Graphics.hpp>
#include "../shared/tcp.hpp"
#include "../shared/packet.hpp"
#include "../engine/engine.hpp"

#include <thread>
#include <atomic>
#include <iostream>

using namespace bombarena;

class BombArenaClientGUI {
private:
    TCPConnection conn;
    std::atomic<bool> running{true};
    std::atomic<bool> gameStarted{false};

    GameState state;
    int playerId = -1;

    sf::RenderWindow window;
    const int TILE = 40;

    bool isHost = false;          
    bool sentStartRequest = false;

public:
    BombArenaClientGUI(const std::string& ip, int port, int is_host)
        : window(sf::VideoMode(600, 600), "BombArena GUI")
    {
        if (!conn.connectToServer(ip, port)) {
            std::cout << "[GUI] Cannot connect\n";
            exit(1);
        }

        std::cout << "[GUI] Connected. Waiting for JOIN_GAME...\n";
        std::cout << "isHost: " << is_host <<"\n";
        isHost = is_host;
        state = initTwoPlayerDefault();
        state.players.clear();
        state.bombs.clear();
        state.turnNumber = 0;
    }

    void start() {
        std::thread net(&BombArenaClientGUI::networkThread, this);
        gameLoop();
        net.join();
    }

private:
    void networkThread() {
        while (running) {
            Packet p;
            if (!conn.recvPacket(p)) {
                running = false;
                break;
            }

            switch (p.type) {

                case PacketType::JOIN_GAME:
                    playerId = p.data["player_id"];
                    std::cout << "[GUI] You are player " << playerId << "\n";
                    break;

                case PacketType::PLAYER_START_GAME:
                    std::cout << "[GUI] START signal received\n";
                    gameStarted = true;
                    break;

                case PacketType::STATE_UPDATE:
                    updateState(p.data);
                    break;

                case PacketType::GAME_END:
                    showGameEnd(p.data);
                    break;

                default:
                    break;
            }
        }
    }

    void updateState(const nlohmann::json& d) {
        state.turnNumber = d["turn"];

        state.players.clear();
        for (auto& p : d["players"]) {
            PlayerState ps;
            ps.id = p["id"];
            ps.x = p["x"];
            ps.y = p["y"];
            ps.alive = p["alive"];
            state.players.push_back(ps);
        }

        state.bombs.clear();
        for (auto& b : d["bombs"]) {
            Bomb bb;
            bb.x = b["x"];
            bb.y = b["y"];
            bb.timer = b["timer"];
            if (b.contains("ownerId")) bb.ownerId = b["ownerId"];
            if (b.contains("range"))   bb.range   = b["range"];
            state.bombs.push_back(bb);
        }
        state.lastExplosionCells.clear();
        if (d.contains("explosions")) {
            for (auto &e : d["explosions"]) {
                int ex = e["x"];
                int ey = e["y"];
                state.lastExplosionCells.emplace_back(ex, ey);
            }
        }
    }


    void gameLoop() {
        while (window.isOpen() && running) {
            handleInput();   
            draw();
            sf::sleep(sf::milliseconds(16));
        }
    }


    void handleInput() {

        sf::Event e;
        while (window.pollEvent(e)) {

            if (e.type == sf::Event::Closed) {
                window.close();
                running = false;
                return;
            }

            if (!gameStarted && isHost && !sentStartRequest) {
                if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::G) {
                    Packet p;
                    p.type = PacketType::PLAYER_START_GAME;
                    conn.sendPacket(p);
                    sentStartRequest = true;
                    std::cout << "[GUI] Sent START request\n";
                }
            }

            if (!gameStarted){
                continue;
            }


            if (!window.hasFocus())
                continue;

            if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape) {
                std::cout<<"Escape!\n";
                window.close();
                running = false;
                return;
            }
            if (e.type == sf::Event::KeyPressed) {

                Packet p;
                p.type = PacketType::PLAYER_ACTION;

                if      (e.key.code == sf::Keyboard::W)     p.data["action"] = "w";
                else if (e.key.code == sf::Keyboard::S)     p.data["action"] = "s";
                else if (e.key.code == sf::Keyboard::A)     p.data["action"] = "a";
                else if (e.key.code == sf::Keyboard::D)     p.data["action"] = "d";
                else if (e.key.code == sf::Keyboard::Space) p.data["action"] = "b";
                else if (e.key.code == sf::Keyboard::B) p.data["action"] = "b";
                else continue;

                conn.sendPacket(p);
            }


        }
    }




    void draw() {
        window.clear(sf::Color(20, 20, 20));

        if (!gameStarted) {
            drawWaitingRoom();
            window.display();
            return;
        }

        drawGame();
        window.display();
    }


    void drawWaitingRoom() {
        sf::Font font;
        font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

        sf::Text t;
        t.setFont(font);
        t.setCharacterSize(28);
        t.setFillColor(sf::Color::White);

        t.setString("Waiting for players...\n");

        if (isHost)
            t.setString("Waiting for players...\nPress G to start the game");

        t.setPosition(50, 200);
        window.draw(t);
    }


    void drawGame() {

        sf::Font font;
        font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

        sf::Text info;
        info.setFont(font);
        info.setCharacterSize(18);
        info.setFillColor(sf::Color::White);

        std::string msg;
        msg += "Player " + std::to_string(playerId) + "\n";
        msg += "Controls:\n";
        msg += "  W/A/S/D = Move\n";
        msg += "  B/SPACE = Place Bomb\n";
        msg += "  (Host Only) G = Start Game\n";
        msg += "Turn: " + std::to_string(state.turnNumber) + "\n";

        info.setString(msg);
        info.setPosition(10, state.height * TILE + 5); 

        window.draw(info);
        sf::RectangleShape tile(sf::Vector2f(TILE - 2, TILE - 2));


        for (int y = 0; y < state.height; ++y) {
            for (int x = 0; x < state.width; ++x) {
                tile.setPosition(x * TILE, y * TILE);

                CellType c = state.cells[y * state.width + x];
                if (c == CellType::Wall)
                    tile.setFillColor(sf::Color(100, 100, 100));
                else
                    tile.setFillColor(sf::Color(50, 50, 50));

                window.draw(tile);
            }
        }

        for (auto& b : state.bombs) {
            sf::CircleShape bombShape(TILE * 0.4f);
            bombShape.setFillColor(sf::Color(255, 200, 0));
            bombShape.setOutlineColor(sf::Color::Black);
            bombShape.setOutlineThickness(2);
            bombShape.setPosition(b.x * TILE + TILE * 0.1f,
                                b.y * TILE + TILE * 0.1f);
            window.draw(bombShape);
        }


        for (auto &c : state.lastExplosionCells) {
            sf::RectangleShape exp(sf::Vector2f(TILE - 6, TILE - 6));
            exp.setFillColor(sf::Color(255, 120, 0, 180)); 
            exp.setPosition(c.first * TILE + 3, c.second * TILE + 3);
            window.draw(exp);
        }

        for (auto& p : state.players) {
            if (!p.alive) continue;

            if (p.id == playerId)
                tile.setFillColor(sf::Color::Green);
            else
                tile.setFillColor(sf::Color::Red);

            tile.setPosition(p.x * TILE, p.y * TILE);
            window.draw(tile);
        }
    }


    void showGameEnd(const nlohmann::json& d) {
        std::cout << "\n===== GAME OVER =====\n";
        if (d["result"] == "draw")
            std::cout << "DRAW!\n";
        else {
            int w = d["winner"];
            if (w == playerId) std::cout << "YOU WIN!\n";
            else std::cout << "You lose. Winner = Player " << w << "\n";
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: bombarena_client_gui <ip> <port> <is_host>\n";
        return 1;
    }
    std::cout<<"receiving start\n";
    int is_host = std::stoi(argv[3]);
    BombArenaClientGUI gui(argv[1], std::stoi(argv[2]), is_host);
    std::cout<<"start bombarenaclient with args:"<< argv[1]<<" "<<argv[2]<<" "<<argv[3]<<"\n";
    gui.start();
    return 0;
}
