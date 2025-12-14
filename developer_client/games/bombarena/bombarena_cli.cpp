// games/bombarena/bombarena_cli.cpp
#include "bombarena_logic.hpp"
#include <iostream>
#include <cctype>

using namespace bombarena;

static ActionType parseActionChar(char c) {
    c = std::tolower(static_cast<unsigned char>(c));
    switch (c) {
        case 'w': return ActionType::MoveUp;
        case 's': return ActionType::MoveDown;
        case 'a': return ActionType::MoveLeft;
        case 'd': return ActionType::MoveRight;
        case 'b': return ActionType::PlaceBomb;
        case 'x': return ActionType::Stay;
        default:  return ActionType::Stay;
    }
}

int main() {
    GameState st = initTwoPlayerDefault();

    std::cout << "=== BombArena CLI ===\n";
    std::cout << "Controls:\n";
    std::cout << "  Player 1 (id=1):  WASD = move, B = bomb, X = stay\n";
    std::cout << "  Player 2 (id=2):  WASD = move, B = bomb, X = stay (same keys, but you enter separately)\n";
    std::cout << "Each turn: P1 chooses, then P2 chooses.\n\n";

    while (true) {
        std::cout << renderBoard(st) << "\n";

        // Quick status
        const PlayerState *p1 = findPlayer(st, 1);
        const PlayerState *p2 = findPlayer(st, 2);

        if (!p1 || !p2) {
            std::cout << "Internal error: missing player.\n";
            return 1;
        }

        if (!p1->alive || !p2->alive) {
            GameResult gr;
            int aliveCount = (p1->alive ? 1 : 0) + (p2->alive ? 1 : 0);
            if (aliveCount == 1) {
                int winner = p1->alive ? 1 : 2;
                gr = {GameResultType::PlayerWin, winner};
            } else {
                gr = {GameResultType::Draw, -1};
            }

            if (gr.type == GameResultType::PlayerWin) {
                std::cout << "Player " << gr.winnerId << " wins!\n";
            } else {
                std::cout << "Draw!\n";
            }
            break;
        }

        char c1, c2;
        std::cout << "Player 1 action (WASD/B/X): ";
        std::cin >> c1;
        std::cout << "Player 2 action (WASD/B/X): ";
        std::cin >> c2;

        std::vector<PlayerAction> acts;
        acts.push_back(PlayerAction{1, parseActionChar(c1)});
        acts.push_back(PlayerAction{2, parseActionChar(c2)});

        GameResult res = step(st, acts);

        if (res.type == GameResultType::PlayerWin) {
            std::cout << renderBoard(st) << "\n";
            std::cout << "Player " << res.winnerId << " wins!\n";
            break;
        } else if (res.type == GameResultType::Draw) {
            std::cout << renderBoard(st) << "\n";
            std::cout << "Draw game.\n";
            break;
        }
    }

    std::cout << "Game over.\n";
    return 0;
}
