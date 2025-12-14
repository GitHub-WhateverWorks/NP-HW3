#pragma once
#include <vector>
#include <string>
#include <utility>

namespace bombarena {



enum class CellType {
    Empty,
    Wall
};

enum class ActionType {
    Stay,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    PlaceBomb
};

enum class GameResultType {
    Ongoing,
    PlayerWin,
    Draw
};



struct PlayerState {
    int id = -1;
    int x = 0;
    int y = 0;
    bool alive = true;
    int bombRange = 3;   
};

struct Bomb {
    int x = 0;
    int y = 0;
    int ownerId = -1;    
    int timer = 3;
    int range = 3;       
};

struct PlayerAction {
    int playerId = -1;
    ActionType type = ActionType::Stay;   
};


struct GameResult {
    GameResultType type = GameResultType::Ongoing;
    int winnerId = -1;
};

struct GameState {
    int width = 11;
    int height = 11;

    std::vector<CellType> cells;

    std::vector<PlayerState> players;

    std::vector<Bomb> bombs;

    int turnNumber = 0;

    std::vector<std::pair<int,int>> lastExplosionCells;
};



GameState initTwoPlayerDefault();
GameResult step(GameState &st, const std::vector<PlayerAction>& actions);
std::string renderBoard(const GameState &st);

} 
