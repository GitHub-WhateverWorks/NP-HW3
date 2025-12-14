#include "engine.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>

namespace bombarena {

static bool inBounds(const GameState &st, int x, int y) {
    return x >= 0 && y >= 0 && x < st.width && y < st.height;
}

CellType getCell(const GameState &state, int x, int y) {
    if (!inBounds(state, x, y)) return CellType::Wall; // treat out-of-bounds as wall
    return state.cells[y * state.width + x];
}

void setCell(GameState &state, int x, int y, CellType t) {
    if (!inBounds(state, x, y)) return;
    state.cells[y * state.width + x] = t;
}

PlayerState* findPlayer(GameState &state, int playerId) {
    for (auto &p : state.players) {
        if (p.id == playerId) return &p;
    }
    return nullptr;
}

const PlayerState* findPlayer(const GameState &state, int playerId) {
    for (auto &p : state.players) {
        if (p.id == playerId) return &p;
    }
    return nullptr;
}


GameState initTwoPlayerDefault() {
    GameState st;
    st.width = 11;
    st.height = 11;
    st.cells.resize(st.width * st.height, CellType::Empty);

    for (int x = 0; x < st.width; ++x) {
        setCell(st, x, 0, CellType::Wall);
        setCell(st, x, st.height - 1, CellType::Wall);
    }
    for (int y = 0; y < st.height; ++y) {
        setCell(st, 0, y, CellType::Wall);
        setCell(st, st.width - 1, y, CellType::Wall);
    }



    PlayerState p1{1, 1, 1, true, 3};
    PlayerState p2{2, st.width - 2, st.height - 2, true, 3};

    st.players.push_back(p1);
    st.players.push_back(p2);

    st.turnNumber = 0;
    return st;
}


static void applyMovement(GameState &st, PlayerState &pl, ActionType act) {
    int dx = 0, dy = 0;
    switch (act) {
        case ActionType::MoveUp:    dy = -1; break;
        case ActionType::MoveDown:  dy =  1; break;
        case ActionType::MoveLeft:  dx = -1; break;
        case ActionType::MoveRight: dx =  1; break;
        default: return;
    }

    int nx = pl.x + dx;
    int ny = pl.y + dy;
    if (!inBounds(st, nx, ny)) return;
    if (getCell(st, nx, ny) == CellType::Wall) return;


    for (const auto &other : st.players) {
        if (other.id != pl.id && other.alive && other.x == nx && other.y == ny) {
            return;
        }
    }

    pl.x = nx;
    pl.y = ny;
}


static void tryPlaceBomb(GameState &st, const PlayerState &pl) {
    std::cout << "tryPlaceBomb called for player=" << pl.id
          << " at (" << pl.x << "," << pl.y << ")\n";

    for (const auto &b : st.bombs) {
        if (b.x == pl.x && b.y == pl.y) return;
    }
    Bomb b;
    b.x = pl.x;
    b.y = pl.y;
    b.ownerId = pl.id;
    b.timer = 10;
    b.range = pl.bombRange;
    st.bombs.push_back(b);
}


static std::vector<std::pair<int,int>> explodeBomb(const GameState &st, const Bomb &b) {
    std::vector<std::pair<int,int>> cells;
    cells.emplace_back(b.x, b.y);

    const int dirs[4][2] = {
        { 1, 0}, {-1, 0}, {0,  1}, {0, -1}
    };

    for (auto &d : dirs) {
        int cx = b.x;
        int cy = b.y;
        for (int r = 0; r < b.range; ++r) {
            cx += d[0];
            cy += d[1];
            if (!inBounds(st, cx, cy)) break;
            if (getCell(st, cx, cy) == CellType::Wall) break;
            cells.emplace_back(cx, cy);
        }
    }

    return cells;
}


static void applyExplosionDamage(GameState &st,
                                 const std::vector<std::pair<int,int>> &explCells) {
    for (auto &p : st.players) {
        if (!p.alive) continue;
        for (auto &c : explCells) {
            if (p.x == c.first && p.y == c.second) {
                p.alive = false;
                break;
            }
        }
    }
}

GameResult step(GameState &st, const std::vector<PlayerAction> &actions) {
    st.turnNumber++;
    st.lastExplosionCells.clear();


    std::vector<ActionType> actionById;
    int maxId = 0;
    for (auto &pl : st.players) {
        maxId = std::max(maxId, pl.id);
    }
    actionById.assign(maxId + 1, ActionType::Stay);
    for (auto &a : actions) {
        if (a.playerId >= 0 && a.playerId < (int)actionById.size()) {
            actionById[a.playerId] = a.type;
        }
    }


    for (auto &pl : st.players) {
        if (!pl.alive) continue;
        ActionType act = actionById[pl.id];


        if (act == ActionType::MoveUp || act == ActionType::MoveDown ||
            act == ActionType::MoveLeft || act == ActionType::MoveRight) {
            applyMovement(st, pl, act);
        }


        if (act == ActionType::PlaceBomb) {
            tryPlaceBomb(st, pl);
        }
    }


    std::vector<Bomb> remaining;
    std::vector<std::pair<int,int>> explosionCellsTotal;

    for (auto &b : st.bombs) {
        Bomb nb = b;
        nb.timer--;
        if (nb.timer <= 0) {
            auto cells = explodeBomb(st, nb);
            explosionCellsTotal.insert(explosionCellsTotal.end(),
                                       cells.begin(), cells.end());
        } else {
            remaining.push_back(nb);
        }
    }
    st.bombs.swap(remaining);
    st.lastExplosionCells = explosionCellsTotal;


    applyExplosionDamage(st, explosionCellsTotal);


    int aliveCount = 0;
    int lastAliveId = -1;
    for (auto &p : st.players) {
        if (p.alive) {
            aliveCount++;
            lastAliveId = p.id;
        }
    }

    if (aliveCount == 1) {
        return GameResult{GameResultType::PlayerWin, lastAliveId};
    }
    if (aliveCount == 0) {
        return GameResult{GameResultType::Draw, -1};
    }

    if (st.turnNumber > 200) {
        return GameResult{GameResultType::Draw, -1};
    }

    return GameResult{GameResultType::Ongoing, -1};
}

std::string renderBoard(const GameState &st) {
    std::vector<std::string> grid(st.height, std::string(st.width, ' '));


    for (int y = 0; y < st.height; ++y) {
        for (int x = 0; x < st.width; ++x) {
            char ch = '.';
            if (getCell(st, x, y) == CellType::Wall) ch = '#';
            grid[y][x] = ch;
        }
    }


    for (const auto &b : st.bombs) {
        if (inBounds(st, b.x, b.y)) grid[b.y][b.x] = '*';
    }


    for (auto &c : st.lastExplosionCells) {
        if (inBounds(st, c.first, c.second)) {
            grid[c.second][c.first] = 'x';
        }
    }


    for (const auto &p : st.players) {
        if (!p.alive) continue;
        if (!inBounds(st, p.x, p.y)) continue;
        char ch = (p.id < 10) ? char('0' + p.id) : 'P';
        grid[p.y][p.x] = ch;
    }

    std::ostringstream oss;
    oss << "Turn " << st.turnNumber << "\n";
    for (int y = 0; y < st.height; ++y) {
        for (int x = 0; x < st.width; ++x) {
            oss << grid[y][x];
        }
        oss << "\n";
    }
    return oss.str();
}

} 
