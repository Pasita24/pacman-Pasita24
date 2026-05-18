#ifndef INKYBTCONTROLLER_H_
#define INKYBTCONTROLLER_H_

#include "Controller.h"
#include "BehaviorTree.h"
#include <chrono>
#include <random>

// Singleton para compartir información entre nodos del árbol
class InkyInfo {
    static InkyInfo *instance;
    InkyInfo() {}
public:
    static InkyInfo* getInfo() {
        if (instance == nullptr) instance = new InkyInfo();
        return instance;
    }
    const GameState* in_gamestate;
    Move out_move;
    std::shared_ptr<Character> in_character;
};

class InkyBTController : public Controller {
private:
    std::shared_ptr<Composite> root;
public:
    InkyBTController(std::shared_ptr<Character> character);
    virtual ~InkyBTController();
    virtual Move getMove(const GameState& gs) override;
};

// Nodo condición: ¿fantasma comestible?
class InkyPowerpill : public Behavior {
public:
    virtual Status update() override;
};

// Nodo acción: Frightened (huida aleatoria)
class InkyFrightened : public Behavior {
private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
public:
    InkyFrightened();
    virtual Status update() override;
};

// Nodo condición: ¿modo dispersión activo? (ciclo de 27s: 7s scatter, 20s chase)
class InkyScatterMode : public Behavior {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> lastSwitch;
    bool scatterActive;
public:
    InkyScatterMode();
    virtual Status update() override;
};

// Nodo acción: Scatter (ir a esquina inferior derecha)
class InkyScatter : public Behavior {
private:
    std::pair<int, int> cornerTarget;
public:
    InkyScatter();
    virtual Status update() override;
};

// Nodo acción: Chase (persecución con intercepción usando Blinky)
class InkyChase : public Behavior {
public:
    virtual Status update() override;
};

#endif // INKYBTCONTROLLER_H_