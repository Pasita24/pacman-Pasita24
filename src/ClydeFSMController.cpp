#include "ClydeFSMController.h"
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

// --------------------------- ClydeFSMController ---------------------------
ClydeFSMController::ClydeFSMController(std::shared_ptr<Character> character)
    : Controller(character), fsm(std::make_shared<ClydeStateMachine>(character)) {}

ClydeFSMController::~ClydeFSMController() {}

Move ClydeFSMController::getMove(const GameState& game) {
    return fsm->update(game);
}

// --------------------------- ClydeNonFrightenedState ---------------------------
ClydeNonFrightenedState::ClydeNonFrightenedState(std::shared_ptr<Character> ch)
    : FSMState(ch),
      chaseState(std::make_shared<ClydeChaseState>(ch)),
      scatterState(std::make_shared<ClydeScatterState>(ch)),
      toScatter(std::make_shared<ClydeTimeOutTransition>(scatterState, true)),
      toChase(std::make_shared<ClydeTimeOutTransition>(chaseState, false)) {
    currentSubstate = chaseState;
    chaseState->addTransition(toScatter);
    scatterState->addTransition(toChase);
}

void ClydeNonFrightenedState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Clyde FSM: Entrando a NonFrightened (subestado Chase)" << std::endl;
    toScatter->reset();
    toChase->reset();
    currentSubstate = chaseState;
    currentSubstate->onEnter(gs);
}

Move ClydeNonFrightenedState::onUpdate(const GameState& gs) {
    auto trans = currentSubstate->getActiveTransition(gs);
    if (trans != nullptr) {
        currentSubstate->onExit(gs);
        trans->onTransition(gs);
        currentSubstate = trans->getNextState();
        currentSubstate->onEnter(gs);
    }
    return currentSubstate->onUpdate(gs);
}

void ClydeNonFrightenedState::onExit(const GameState& gs) {
    currentSubstate->onExit(gs);
    if (!nogui) std::cout << "Clyde FSM: Saliendo de NonFrightened" << std::endl;
}

void ClydeNonFrightenedState::switchToChase() { currentSubstate = chaseState; }
void ClydeNonFrightenedState::switchToScatter() { currentSubstate = scatterState; }
ClydeNonFrightenedState::~ClydeNonFrightenedState() {}

// --------------------------- ClydeFrightenedState ---------------------------
ClydeFrightenedState::ClydeFrightenedState(std::shared_ptr<Character> ch)
    : FSMState(ch),
      rng(std::chrono::steady_clock::now().time_since_epoch().count()),
      dist(0, 3) {}

void ClydeFrightenedState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Clyde FSM: Entrando a Frightened (huyendo aleatoriamente)" << std::endl;
}

Move ClydeFrightenedState::onUpdate(const GameState& gs) {
    auto moves = getLegalMoves(character, &gs);
    if (moves.empty()) return PASS;
    int idx = dist(rng) % moves.size();
    return moves[idx];
}

void ClydeFrightenedState::onExit(const GameState& gs) {
    if (!nogui) std::cout << "Clyde FSM: Saliendo de Frightened" << std::endl;
}
ClydeFrightenedState::~ClydeFrightenedState() {}

// --------------------------- ClydeChaseState (con comportamiento condicional) ---------------------------
ClydeChaseState::ClydeChaseState(std::shared_ptr<Character> ch) : FSMState(ch) {
    // Esquina inferior izquierda: coordenadas (0, altura máxima del laberinto)
    // Como no conocemos la altura exacta, usamos (0, 600) que es un valor razonable
    // En el mapa original, la esquina inferior izquierda suele ser (0, ~512)
    escapeCorner = {0, 600};
}

void ClydeChaseState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Clyde FSM: Subestado Chase activo (condicional)" << std::endl;
}

Move ClydeChaseState::onUpdate(const GameState& gs) {
    auto myPos = character->getPos();
    auto myCoords = gs.getMaze().getNodePos(myPos);
    int pacmanNode = gs.getPacmanPos();
    auto pacmanCoords = gs.getMaze().getNodePos(pacmanNode);

    // Calcular distancia euclídea al cuadrado entre Clyde y Pac-Man
    float dx = myCoords.first - pacmanCoords.first;
    float dy = myCoords.second - pacmanCoords.second;
    float distSq = dx*dx + dy*dy;

    // Umbral: 8 casillas -> 8*8 = 64 píxeles cada lado? Una casilla son 8 píxeles (como en Pinky)
    // Distancia al cuadrado: (8*8)^2? No: distancia en píxeles = 8*8 = 64, entonces distSq = 64*64 = 4096
    const float THRESHOLD_SQ = 64.0f * 64.0f; // 4096

    std::pair<int, int> target;
    bool isFleeing = false;

    if (distSq > THRESHOLD_SQ) {
        // Lejos: perseguir a Pac-Man
        target = pacmanCoords;
        if (!nogui) std::cout << "Clyde: Lejos de Pac-Man, persiguiendo." << std::endl;
    } else {
        // Cerca: huir a la esquina inferior izquierda
        target = escapeCorner;
        isFleeing = true;
        if (!nogui) std::cout << "Clyde: Cerca de Pac-Man, huyendo a la esquina." << std::endl;
    }

    auto moves = getLegalMoves(character, &gs);
    if (moves.empty()) return PASS;

    Move bestMove = moves[0];
    float bestDist = std::numeric_limits<float>::max();
    for (Move m : moves) {
        if (m == PASS) continue;
        int neighbor = gs.getMaze().getNeighbour(character->getPos(), m);
        auto pos = gs.getMaze().getNodePos(neighbor);
        float dx = pos.first - target.first;
        float dy = pos.second - target.second;
        float distSq = dx*dx + dy*dy;
        if (distSq < bestDist) {
            bestDist = distSq;
            bestMove = m;
        }
    }
    return bestMove;
}

ClydeChaseState::~ClydeChaseState() {}

// --------------------------- ClydeScatterState (esquina inferior izquierda) ---------------------------
ClydeScatterState::ClydeScatterState(std::shared_ptr<Character> ch) : FSMState(ch) {
    cornerTarget = {0, 600}; // misma esquina inferior izquierda
}

void ClydeScatterState::onEnter(const GameState& gs) {
    if (!nogui) std::cout << "Clyde FSM: Subestado Scatter activo (yendo a esquina inferior izquierda)" << std::endl;
}

Move ClydeScatterState::onUpdate(const GameState& gs) {
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

ClydeScatterState::~ClydeScatterState() {}

// --------------------------- ClydeTimeOutTransition ---------------------------
ClydeTimeOutTransition::ClydeTimeOutTransition(std::shared_ptr<FSMState> next, bool toScatter)
    : lastSwitch(std::chrono::high_resolution_clock::now()), active(toScatter), nextState(next) {}

bool ClydeTimeOutTransition::isValid(const GameState& gs) {
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - lastSwitch;
    float cycleTime = fmod(elapsed.count(), 27.0f);
    bool shouldBeScatter = (cycleTime < 7.0f);
    return ((active && shouldBeScatter) || (!active && !shouldBeScatter));
}

std::shared_ptr<FSMState> ClydeTimeOutTransition::getNextState() { return nextState; }
void ClydeTimeOutTransition::reset() { lastSwitch = std::chrono::high_resolution_clock::now(); }

// --------------------------- ClydeStateMachine (top-level) ---------------------------
ClydeStateMachine::ClydeStateMachine(std::shared_ptr<Character> ch)
    : FiniteStateMachine(ch), firstUpdate(true) {
    auto nonFright = std::make_shared<ClydeNonFrightenedState>(ch);
    auto fright = std::make_shared<ClydeFrightenedState>(ch);
    states.push_back(nonFright);
    states.push_back(fright);
    initialState = nonFright;
    activeState = initialState;
}

Move ClydeStateMachine::update(const GameState& gs) {
    if (firstUpdate) {
        activeState->onEnter(gs);
        firstUpdate = false;
    }
    auto ghost = dynamic_cast<Ghost*>(character.get());
    bool edible = (ghost != nullptr && ghost->isEdible());
    std::shared_ptr<FSMState> nextState = nullptr;
    if (edible && activeState != states.back())
        nextState = states.back(); // Frightened
    else if (!edible && activeState == states.back())
        nextState = states.front(); // NonFrightened
    if (nextState != nullptr) {
        activeState->onExit(gs);
        activeState = nextState;
        activeState->onEnter(gs);
    }
    return activeState->onUpdate(gs);
}
ClydeStateMachine::~ClydeStateMachine() {}