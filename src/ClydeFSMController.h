#ifndef CLYDEFSMCONTROLLER_H_
#define CLYDEFSMCONTROLLER_H_

#include "Controller.h"
#include "FSM.h"
#include <chrono>
#include <random>

// Declaraciones adelantadas
class ClydeNonFrightenedState;
class ClydeFrightenedState;
class ClydeChaseState;
class ClydeScatterState;
class ClydeTimeOutTransition;
class ClydeStateMachine;

class ClydeFSMController : public Controller {
private:
    std::shared_ptr<ClydeStateMachine> fsm;
public:
    ClydeFSMController(std::shared_ptr<Character> character);
    virtual ~ClydeFSMController();
    virtual Move getMove(const GameState& game) override;
};

class ClydeNonFrightenedState : public FSMState {
private:
    std::shared_ptr<FSMState> currentSubstate;
    std::shared_ptr<ClydeChaseState> chaseState;
    std::shared_ptr<ClydeScatterState> scatterState;
    std::shared_ptr<ClydeTimeOutTransition> toScatter;
    std::shared_ptr<ClydeTimeOutTransition> toChase;
public:
    ClydeNonFrightenedState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    virtual void onExit(const GameState& gs) override;
    void switchToChase();
    void switchToScatter();
    ~ClydeNonFrightenedState();
};

class ClydeFrightenedState : public FSMState {
private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
public:
    ClydeFrightenedState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    virtual void onExit(const GameState& gs) override;
    ~ClydeFrightenedState();
};

class ClydeChaseState : public FSMState {
private:
    std::pair<int, int> escapeCorner; // esquina inferior izquierda para huir
public:
    ClydeChaseState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    ~ClydeChaseState();
};

class ClydeScatterState : public FSMState {
private:
    std::pair<int, int> cornerTarget; // misma esquina inferior izquierda
public:
    ClydeScatterState(std::shared_ptr<Character> ch);
    virtual void onEnter(const GameState& gs) override;
    virtual Move onUpdate(const GameState& gs) override;
    ~ClydeScatterState();
};

class ClydeTimeOutTransition : public FSMTransition {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> lastSwitch;
    bool active; // true -> Scatter, false -> Chase
    std::shared_ptr<FSMState> nextState;
public:
    ClydeTimeOutTransition(std::shared_ptr<FSMState> next, bool toScatter);
    virtual bool isValid(const GameState& gs) override;
    virtual std::shared_ptr<FSMState> getNextState() override;
    void reset();
};

class ClydeStateMachine : public FiniteStateMachine {
private:
    bool firstUpdate;
public:
    ClydeStateMachine(std::shared_ptr<Character> ch);
    virtual Move update(const GameState& gs) override;
    ~ClydeStateMachine();
};

#endif // CLYDEFSMCONTROLLER_H_