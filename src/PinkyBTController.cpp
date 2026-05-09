#include "PinkyBTController.h"
#include <cmath>
#include <iostream>

PinkyInfo* PinkyInfo::instance = nullptr;

// -------------------- Constructor del árbol --------------------
PinkyBTController::PinkyBTController(std::shared_ptr<Character> character)
    : Controller(character), root(std::make_shared<Selector>()) {

    // Nodo raíz Selector: prueba cada hijo en orden hasta que uno tenga éxito

    // 1. Si es comestible -> Frightened (huir)
    auto frightenedFilter = std::make_shared<Filter>();
    frightenedFilter->addCondition(std::make_shared<PinkyPowerpill>());
    frightenedFilter->addAction(std::make_shared<PinkyFrightened>());
    root->addChild(frightenedFilter);

    // 2. Si está en modo dispersión -> Scatter
    auto scatterFilter = std::make_shared<Filter>();
    scatterFilter->addCondition(std::make_shared<PinkyScatterMode>());
    scatterFilter->addAction(std::make_shared<PinkyScatter>());
    root->addChild(scatterFilter);

    // 3. Por defecto -> Chase
    root->addChild(std::make_shared<PinkyChase>());
}

PinkyBTController::~PinkyBTController() {}

Move PinkyBTController::getMove(const GameState& gs) {
    PinkyInfo::getInfo()->in_character = character;
    PinkyInfo::getInfo()->in_gamestate = &gs;
    root->tick();
    return PinkyInfo::getInfo()->out_move;
}

// -------------------- Nodo: condición comestible --------------------
Status PinkyPowerpill::update() {
    auto character = PinkyInfo::getInfo()->in_character;
    auto ghost = dynamic_cast<Ghost*>(character.get());
    if (ghost != nullptr && ghost->isEdible())
        return BH_SUCCESS;
    return BH_FAILURE;
}

// -------------------- Nodo: Frightened (huida aleatoria) --------------------
PinkyFrightened::PinkyFrightened()
    : rng(std::chrono::steady_clock::now().time_since_epoch().count()),
      dist(0, 3) {}

Status PinkyFrightened::update() {
    auto character = PinkyInfo::getInfo()->in_character;
    auto gs = PinkyInfo::getInfo()->in_gamestate;

    std::vector<Move> moves;
    if (character->getDirection() == PASS)
        moves = gs->getMaze().getPossibleMoves(character->getPos());
    else
        moves = gs->getMaze().getGhostLegalMoves(character->getPos(), character->getDirection());

    if (moves.empty()) {
        PinkyInfo::getInfo()->out_move = PASS;
        return BH_FAILURE;
    }
    int idx = dist(rng) % moves.size();
    PinkyInfo::getInfo()->out_move = moves[idx];
    return BH_SUCCESS;
}

// -------------------- Nodo: condición de modo dispersión cíclico --------------------
PinkyScatterMode::PinkyScatterMode()
    : lastSwitch(std::chrono::high_resolution_clock::now()),
      scatterActive(true) {}

Status PinkyScatterMode::update() {
    // Ciclo: 7 segundos Scatter, 20 segundos Chase (como en Pacman original)
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - lastSwitch;
    float cycleTime = fmod(elapsed.count(), 27.0f); // ciclo total 27s
    if (cycleTime < 7.0f)
        scatterActive = true;
    else
        scatterActive = false;

    return scatterActive ? BH_SUCCESS : BH_FAILURE;
}

// -------------------- Nodo: Scatter (ir a esquina superior izquierda) --------------------
PinkyScatter::PinkyScatter() {
    // Esquina superior izquierda: coordenadas (0,0) en píxeles
    cornerTarget = std::make_pair(0, 0);
}

Status PinkyScatter::update() {
    auto character = PinkyInfo::getInfo()->in_character;
    auto gs = PinkyInfo::getInfo()->in_gamestate;

    std::vector<Move> moves;
    if (character->getDirection() == PASS)
        moves = gs->getMaze().getPossibleMoves(character->getPos());
    else
        moves = gs->getMaze().getGhostLegalMoves(character->getPos(), character->getDirection());

    if (moves.empty()) {
        PinkyInfo::getInfo()->out_move = PASS;
        return BH_FAILURE;
    }

    // Elegir movimiento que minimice distancia a la esquina
    Move bestMove = PASS;
    float bestDist = 1e9f;
    for (Move m : moves) {
        if (m == PASS) continue;
        int neighbor = gs->getMaze().getNeighbour(character->getPos(), m);
        auto pos = gs->getMaze().getNodePos(neighbor);
        float dx = pos.first - cornerTarget.first;
        float dy = pos.second - cornerTarget.second;
        float distSq = dx*dx + dy*dy;
        if (distSq < bestDist) {
            bestDist = distSq;
            bestMove = m;
        }
    }
    PinkyInfo::getInfo()->out_move = bestMove;
    return BH_SUCCESS;
}

// -------------------- Nodo: Chase (apuntar 4 casillas adelante de PacMan) --------------------
Status PinkyChase::update() {
    auto character = PinkyInfo::getInfo()->in_character;
    auto gs = PinkyInfo::getInfo()->in_gamestate;

    // Posición actual de PacMan
    int pacmanNode = gs->getPacmanPos();
    auto pacmanPos = gs->getMaze().getNodePos(pacmanNode);
    
    // Convertir explícitamente el int devuelto a tipo Move
    Move pacmanDir = static_cast<Move>(gs->getPacmanDir());

    // Calcular desplazamiento de 4 casillas (32 píxeles) en la dirección de PacMan
    int offsetX = 0, offsetY = 0;
    switch (pacmanDir) {
        case UP:    offsetY = -32; break;
        case DOWN:  offsetY =  32; break;
        case LEFT:  offsetX = -32; break;
        case RIGHT: offsetX =  32; break;
        default: break;
    }
    std::pair<int, int> targetPos = {pacmanPos.first + offsetX, pacmanPos.second + offsetY};

    // Obtener movimientos legales del fantasma
    std::vector<Move> moves;
    if (character->getDirection() == PASS)
        moves = gs->getMaze().getPossibleMoves(character->getPos());
    else
        moves = gs->getMaze().getGhostLegalMoves(character->getPos(), character->getDirection());

    if (moves.empty()) {
        PinkyInfo::getInfo()->out_move = PASS;
        return BH_FAILURE;
    }

    // Elegir movimiento que minimice distancia al punto objetivo
    Move bestMove = PASS;
    float bestDist = 1e9f;
    for (Move m : moves) {
        if (m == PASS) continue;
        int neighbor = gs->getMaze().getNeighbour(character->getPos(), m);
        auto pos = gs->getMaze().getNodePos(neighbor);
        float dx = pos.first - targetPos.first;
        float dy = pos.second - targetPos.second;
        float distSq = dx*dx + dy*dy;
        if (distSq < bestDist) {
            bestDist = distSq;
            bestMove = m;
        }
    }
    PinkyInfo::getInfo()->out_move = bestMove;
    return BH_SUCCESS;
}