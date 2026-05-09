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
};

// Controlador principal
class PacmanBTController : public Controller {
private:
    std::shared_ptr<Composite> root;
public:
    PacmanBTController(std::shared_ptr<Character> character);
    virtual ~PacmanBTController();
    virtual Move getMove(const GameState& gs) override;
};

// Nodo condición: ¿hay fantasmas comestibles cerca?
class NearbyEdibleGhost : public Behavior {
public:
    virtual Status update() override;
};

// Nodo acción: perseguir al fantasma comestible más cercano
class ChaseEdibleGhost : public Behavior {
public:
    virtual Status update() override;
};

// Nodo condición: ¿hay fantasmas no comestibles peligrosos cerca?
class DangerousGhostNearby : public Behavior {
private:
    float threatDistance;
public:
    DangerousGhostNearby(float distance = 64.0f);
    virtual Status update() override;
};

class EvadeGhosts : public Behavior {
public:
    virtual Status update() override;
};

class NearbyFood : public Behavior {
private:
    float searchRadius;
public:
    NearbyFood(float radius = 80.0f);
    virtual Status update() override;
};

// Nodo accion: moverse hacia la comida más cercana
class CollectFood : public Behavior {
public:
    virtual Status update() override;
};

// Nodo accion por defecto: movimiento aleatorio
class RandomExplore : public Behavior {
private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
public:
    RandomExplore();
    virtual Status update() override;
};

#endif // PACMANBTCONTROLLER_H_