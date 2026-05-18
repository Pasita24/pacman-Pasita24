#ifndef PACMANBTCONTROLLER_H_
#define PACMANBTCONTROLLER_H_

#include "Controller.h"
#include "BehaviorTree.h"
#include <memory>
#include <chrono>
#include <random>

// Clase singleton para datos compartidos
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
    
    // Cache de comida
    std::vector<int> cached_food_nodes;
    bool food_cache_valid = false;
    
    // Prevención de oscilaciones: compromiso temporal
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

// ¿Vale la pena perseguir fantasma comestible? (más cerca que comida)
class WorthChasingGhost : public Behavior {
public:
    virtual Status update() override;
};

// ¿Peligro INMEDIATO? (fantasma muy cerca)
class ImmediateDanger : public Behavior {
private:
    float criticalDistance;
public:
    ImmediateDanger(float distance = 35.0f);
    virtual Status update() override;
};

// ¿Vale la pena ir por Power Pill? (peligro cercano + power pill accesible)
class WorthGettingPowerPill : public Behavior {
public:
    virtual Status update() override;
};

// ¿Queda comida?
class FoodRemaining : public Behavior {
public:
    virtual Status update() override;
};

// === ACCIONES ===

// Perseguir fantasma comestible cercano
class ChaseNearbyGhost : public Behavior {
public:
    virtual Status update() override;
};

// Huir urgentemente
class EmergencyEvade : public Behavior {
public:
    virtual Status update() override;
};

// Ir por Power Pill estratégicamente
class GetStrategicPowerPill : public Behavior {
public:
    virtual Status update() override;
};

// Recolectar comida (acción principal)
class CollectNearestFood : public Behavior {
public:
    virtual Status update() override;
};

// Exploración con compromiso (anti-oscilación)
class CommittedExplore : public Behavior {
private:
    std::mt19937 rng;
public:
    CommittedExplore();
    virtual Status update() override;
};

#endif // PACMANBTCONTROLLER_H_