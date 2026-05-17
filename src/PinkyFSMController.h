#ifndef PINKYFSMCONTROLLER_H_
#define PINKYFSMCONTROLLER_H_

#include "Controller.h"
#include "FSM.h"
#include <chrono>
#include <random>

// Declaraciones adelantadas
class PinkyNonFrightenedState;
class PinkyFrightenedState;
class PinkyChaseState;
class PinkyScatterState;
class PinkyTimeOutTransition;
class PinkyStateMachine;

class PinkyFSMController : public Controller {
private:
    std::shared_ptr<PinkyStateMachine> fsm;
public:
    PinkyFSMController(std::shared_ptr<Character> character);
    virtual ~PinkyFSMController();
    virtual Move getMove(const GameState& game) override;
};

class PinkyNonFrightenedState : public FSMState {
private:
    std::shared_ptr<FSMState> currentSubstate;
    std::shared_ptr<PinkyChaseState> chaseState;
    std::shared_ptr<PinkyScatterState> scatterState;
    std::shared_ptr<PinkyTimeOutTransition> toScatter;
    std::shared_ptr<PinkyTimeOutTransition> toChase;
public:
    PinkyNonFrightenedState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    virtual void onExit(const GameState& gs) override;
    void switchToChase();
    void switchToScatter();
    ~PinkyNonFrightenedState();
};

class PinkyFrightenedState : public FSMState {
private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
public:
    PinkyFrightenedState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    virtual void onExit(const GameState& gs) override;
    ~PinkyFrightenedState();
};

class PinkyChaseState : public FSMState {
public:
    PinkyChaseState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    ~PinkyChaseState();
};

class PinkyScatterState : public FSMState {
private:
    std::pair<int, int> cornerTarget;
public:
    PinkyScatterState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    ~PinkyScatterState();
};

class PinkyTimeOutTransition : public FSMTransition {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> lastSwitch;
    bool active;
    std::shared_ptr<FSMState> nextState;
public:
    PinkyTimeOutTransition(std::shared_ptr<FSMState> next, bool toScatter);
    virtual bool isValid(const GameState& gs) override;
    virtual std::shared_ptr<FSMState> getNextState() override;
    void reset();
};

class PinkyStateMachine : public FiniteStateMachine {
private:
    bool firstUpdate;   // ← NUEVO: control de primera actualización
public:
    PinkyStateMachine(std::shared_ptr<Character> ch);
    virtual Move update(const GameState& gs) override;
    ~PinkyStateMachine();
};

#endif