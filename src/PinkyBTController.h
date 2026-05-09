#ifndef PINKYBTCONTROLLER_H_
#define PINKYBTCONTROLLER_H_

#include "Controller.h"
#include "BehaviorTree.h"
#include <chrono>
#include <random>

// Clase singleton para compartir información entre nodos del árbol
class PinkyInfo {
    static PinkyInfo *instance;
    PinkyInfo() {}
public:
    static PinkyInfo* getInfo() {
        if (instance == nullptr) instance = new PinkyInfo();
        return instance;
    }
    const GameState* in_gamestate;
    Move out_move;
    std::shared_ptr<Character> in_character;
};

// Controlador principal
class PinkyBTController : public Controller {
private:
    std::shared_ptr<Composite> root;
public:
    PinkyBTController(std::shared_ptr<Character> character);
    virtual ~PinkyBTController();
    virtual Move getMove(const GameState& gs) override;
};

// Nodo condición: ¿fantasma comestible?
class PinkyPowerpill : public Behavior {
public:
    virtual Status update() override;
};

// Nodo acción: comportamiento de huida aleatoria (Frightened)
class PinkyFrightened : public Behavior {
private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
public:
    PinkyFrightened();
    virtual Status update() override;
};

// Nodo condición: ¿modo dispersión activo? (basado en tiempo cíclico, ej. cada 7 segundos)
class PinkyScatterMode : public Behavior {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> lastSwitch;
    bool scatterActive; // true = modo dispersión, false = modo persecución
public:
    PinkyScatterMode();
    virtual Status update() override;
};

// Nodo acción: moverse hacia la esquina superior izquierda (Scatter)
class PinkyScatter : public Behavior {
private:
    std::pair<int, int> cornerTarget;
public:
    PinkyScatter();
    virtual Status update() override;
};

// Nodo acción: persecución adelantada (Chase)
class PinkyChase : public Behavior {
public:
    virtual Status update() override;
};

#endif // PINKYBTCONTROLLER_H_