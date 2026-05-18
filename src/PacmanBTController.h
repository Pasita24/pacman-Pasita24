#ifndef PACMANBTCONTROLLER_H_
#define PACMANBTCONTROLLER_H_

#include "Controller.h"
#include "BehaviorTree.h"
#include <memory>
#include <chrono>
#include <random>

class PacmanInfo {
    static PacmanInfo* instance;
    PacmanInfo() {}
public:
    static PacmanInfo* getInfo() {
        if (!instance) instance = new PacmanInfo();
        return instance;
    }
    const GameState* in_gamestate;
    Move out_move;
    std::shared_ptr<Character> in_character;

    std::vector<int> cached_food_nodes;
    bool food_cache_valid = false;

    // Anti-oscilación
    Move committed_move = PASS;
    int commitment_ticks = 0;
    int last_position = -1;
};

class PacmanBTController : public Controller {
private:
    std::shared_ptr<Composite> root;
public:
    PacmanBTController(std::shared_ptr<Character> character);
    virtual ~PacmanBTController();
    virtual Move getMove(const GameState& gs) override;
};

// === CONDICIONES ===
class WorthChasingGhost : public Behavior {
public: virtual Status update() override;
};

class ImmediateDanger : public Behavior {
private:
    float criticalDistance;
public:
    ImmediateDanger(float distance = 35.0f);
    virtual Status update() override;
};

class WorthGettingPowerPill : public Behavior {
public: virtual Status update() override;
};

class FoodRemaining : public Behavior {
public: virtual Status update() override;
};

// === ACCIONES ===
class ChaseNearbyGhost : public Behavior {
public: virtual Status update() override;
};

// NUEVA: huye evaluando TODOS los fantasmas
class MultiGhostEvade : public Behavior {
public: virtual Status update() override;
};

class GetStrategicPowerPill : public Behavior {
public: virtual Status update() override;
};

class CollectNearestFood : public Behavior {
public: virtual Status update() override;
};

class CommittedExplore : public Behavior {
private:
    std::mt19937 rng;
public:
    CommittedExplore();
    virtual Status update() override;
};

#endif // PACMANBTCONTROLLER_H_