#include "PinkyFSMController.h"
#include "Ghost.h"
#include <cmath>
#include <iostream>

extern bool nogui;

static std::vector<Move> getLegalMoves(std::shared_ptr<Character> ch, const GameState* gs) {
    if (ch->getDirection() == PASS)
        return gs->getMaze().getPossibleMoves(ch->getPos());
    else
        return gs->getMaze().getGhostLegalMoves(ch->getPos(), ch->getDirection());
}

PinkyFSMController::PinkyFSMController(std::shared_ptr<Character> character)
    : Controller(character), fsm(std::make_shared<PinkyStateMachine>(character)) {}

PinkyFSMController::~PinkyFSMController() {}

Move PinkyFSMController::getMove(const GameState& game) {
    return fsm->update(game);
}

// ---------- NonFrightenedState ----------
PinkyNonFrightenedState::PinkyNonFrightenedState(std::shared_ptr<Character> ch)
    : FSMState(ch),
      chaseState(std::make_shared<PinkyChaseState>(ch)),
      scatterState(std::make_shared<PinkyScatterState>(ch)),
      toScatter(std::make_shared<PinkyTimeOutTransition>(scatterState, true)),
      toChase(std::make_shared<PinkyTimeOutTransition>(chaseState, false)) {
    currentSubstate = chaseState;
    chaseState->addTransition(toScatter);
    scatterState->addTransition(toChase);
}

void PinkyNonFrightenedState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Blinky FSM: Entrando a NonFrightened (subestado Chase)" << std::endl;
    toScatter->reset();
    toChase->reset();
    currentSubstate = chaseState;
    currentSubstate->onEnter(gs);
}

Move PinkyNonFrightenedState::onUpdate(const GameState& gs) {
    auto trans = currentSubstate->getActiveTransition(gs);
    if (trans != nullptr) {
        currentSubstate->onExit(gs);
        trans->onTransition(gs);
        currentSubstate = trans->getNextState();
        currentSubstate->onEnter(gs);
    }
    return currentSubstate->onUpdate(gs);
}

void PinkyNonFrightenedState::onExit(const GameState& gs) {
    currentSubstate->onExit(gs);
    if (!nogui) std::cout << "Blinky FSM: Saliendo de NonFrightened" << std::endl;
}

void PinkyNonFrightenedState::switchToChase() { currentSubstate = chaseState; }
void PinkyNonFrightenedState::switchToScatter() { currentSubstate = scatterState; }
PinkyNonFrightenedState::~PinkyNonFrightenedState() {}

// ---------- FrightenedState ----------
PinkyFrightenedState::PinkyFrightenedState(std::shared_ptr<Character> ch)
    : FSMState(ch),
      rng(std::chrono::steady_clock::now().time_since_epoch().count()),
      dist(0, 3) {}

void PinkyFrightenedState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Blinky FSM: Entrando a Frightened (huyendo aleatoriamente)" << std::endl;
}

Move PinkyFrightenedState::onUpdate(const GameState& gs) {
    auto moves = getLegalMoves(character, &gs);
    if (moves.empty()) return PASS;
    int idx = dist(rng) % moves.size();
    return moves[idx];
}

void PinkyFrightenedState::onExit(const GameState& gs) {
    if (!nogui) std::cout << "Blinky FSM: Saliendo de Frightened" << std::endl;
}
PinkyFrightenedState::~PinkyFrightenedState() {}

// ---------- ChaseState (comportamiento BLINKY) ----------
PinkyChaseState::PinkyChaseState(std::shared_ptr<Character> ch) : FSMState(ch) {}

void PinkyChaseState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Blinky FSM: Subestado Chase activo - persiguiendo directamente a Pac-Man" << std::endl;
}

Move PinkyChaseState::onUpdate(const GameState& gs) {
    int pacmanNode = gs.getPacmanPos();
    auto targetPos = gs.getMaze().getNodePos(pacmanNode);
    auto moves = getLegalMoves(character, &gs);
    if (moves.empty()) return PASS;

    Move bestMove = moves[0];
    float bestDist = std::numeric_limits<float>::max();
    for (Move m : moves) {
        if (m == PASS) continue;
        int neighbor = gs.getMaze().getNeighbour(character->getPos(), m);
        auto pos = gs.getMaze().getNodePos(neighbor);
        float dx = pos.first - targetPos.first;
        float dy = pos.second - targetPos.second;
        float distSq = dx*dx + dy*dy;
        if (distSq < bestDist) {
            bestDist = distSq;
            bestMove = m;
        }
    }
    return bestMove;
}
PinkyChaseState::~PinkyChaseState() {}

// ---------- ScatterState ----------
PinkyScatterState::PinkyScatterState(std::shared_ptr<Character> ch) : FSMState(ch) {
    cornerTarget = {0, 0};
}

void PinkyScatterState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Blinky FSM: Subestado Scatter activo (yendo a esquina superior izquierda)" << std::endl;
}

Move PinkyScatterState::onUpdate(const GameState& gs) {
    auto moves = getLegalMoves(character, &gs);
    if (moves.empty()) return PASS;
    Move bestMove = moves[0];
    float bestDist = std::numeric_limits<float>::max();
    for (Move m : moves) {
        if (m == PASS) continue;
        int neighbor = gs.getMaze().getNeighbour(character->getPos(), m);
        auto pos = gs.getMaze().getNodePos(neighbor);
        float dx = pos.first - cornerTarget.first;
        float dy = pos.second - cornerTarget.second;
        float distSq = dx*dx + dy*dy;
        if (distSq < bestDist) {
            bestDist = distSq;
            bestMove = m;
        }
    }
    return bestMove;
}
PinkyScatterState::~PinkyScatterState() {}

// ---------- TimeOutTransition ----------
PinkyTimeOutTransition::PinkyTimeOutTransition(std::shared_ptr<FSMState> next, bool toScatter)
    : lastSwitch(std::chrono::high_resolution_clock::now()), active(toScatter), nextState(next) {}

bool PinkyTimeOutTransition::isValid(const GameState& gs) {
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - lastSwitch;
    float cycleTime = fmod(elapsed.count(), 27.0f);
    bool shouldBeScatter = (cycleTime < 7.0f);
    return ((active && shouldBeScatter) || (!active && !shouldBeScatter));
}

std::shared_ptr<FSMState> PinkyTimeOutTransition::getNextState() { return nextState; }
void PinkyTimeOutTransition::reset() { lastSwitch = std::chrono::high_resolution_clock::now(); }

// ---------- PinkyStateMachine (top-level) ----------
PinkyStateMachine::PinkyStateMachine(std::shared_ptr<Character> ch)
    : FiniteStateMachine(ch), firstUpdate(true) {
    auto nonFright = std::make_shared<PinkyNonFrightenedState>(ch);
    auto fright = std::make_shared<PinkyFrightenedState>(ch);
    states.push_back(nonFright);
    states.push_back(fright);
    initialState = nonFright;
    activeState = initialState;
}

Move PinkyStateMachine::update(const GameState& gs) {
    if (firstUpdate) {
        activeState->onEnter(gs);
        firstUpdate = false;
    }
    auto ghost = dynamic_cast<Ghost*>(character.get());
    bool edible = (ghost != nullptr && ghost->isEdible());
    std::shared_ptr<FSMState> nextState = nullptr;
    if (edible && activeState != states.back())
        nextState = states.back();
    else if (!edible && activeState == states.back())
        nextState = states.front();
    if (nextState != nullptr) {
        activeState->onExit(gs);
        activeState = nextState;
        activeState->onEnter(gs);
    }
    return activeState->onUpdate(gs);
}
PinkyStateMachine::~PinkyStateMachine() {}