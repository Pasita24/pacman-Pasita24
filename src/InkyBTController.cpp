#include "InkyBTController.h"
#include "Ghost.h"
#include <cmath>
#include <iostream>

extern bool nogui;

InkyInfo* InkyInfo::instance = nullptr;

// -------------------- Constructor del árbol --------------------
InkyBTController::InkyBTController(std::shared_ptr<Character> character)
    : Controller(character), root(std::make_shared<Selector>()) {

    // 1. Si es comestible -> Frightened
    auto frightenedFilter = std::make_shared<Filter>();
    frightenedFilter->addCondition(std::make_shared<InkyPowerpill>());
    frightenedFilter->addAction(std::make_shared<InkyFrightened>());
    root->addChild(frightenedFilter);

    // 2. Si está en modo dispersión -> Scatter
    auto scatterFilter = std::make_shared<Filter>();
    scatterFilter->addCondition(std::make_shared<InkyScatterMode>());
    scatterFilter->addAction(std::make_shared<InkyScatter>());
    root->addChild(scatterFilter);

    // 3. Por defecto -> Chase
    root->addChild(std::make_shared<InkyChase>());
}

InkyBTController::~InkyBTController() {}

Move InkyBTController::getMove(const GameState& gs) {
    InkyInfo::getInfo()->in_character = character;
    InkyInfo::getInfo()->in_gamestate = &gs;
    root->tick();
    return InkyInfo::getInfo()->out_move;
}

// -------------------- Nodo: condición comestible --------------------
Status InkyPowerpill::update() {
    auto character = InkyInfo::getInfo()->in_character;
    auto ghost = dynamic_cast<Ghost*>(character.get());
    if (ghost != nullptr && ghost->isEdible())
        return BH_SUCCESS;
    return BH_FAILURE;
}

// -------------------- Nodo: Frightened (huida aleatoria) --------------------
InkyFrightened::InkyFrightened()
    : rng(std::chrono::steady_clock::now().time_since_epoch().count()),
      dist(0, 3) {}

Status InkyFrightened::update() {
    auto character = InkyInfo::getInfo()->in_character;
    auto gs = InkyInfo::getInfo()->in_gamestate;

    std::vector<Move> moves;
    if (character->getDirection() == PASS)
        moves = gs->getMaze().getPossibleMoves(character->getPos());
    else
        moves = gs->getMaze().getGhostLegalMoves(character->getPos(), character->getDirection());

    if (moves.empty()) {
        InkyInfo::getInfo()->out_move = PASS;
        return BH_FAILURE;
    }
    int idx = dist(rng) % moves.size();
    InkyInfo::getInfo()->out_move = moves[idx];
    if (!nogui) std::cout << "Inky BT: Frightened - huyendo aleatoriamente" << std::endl;
    return BH_SUCCESS;
}

// -------------------- Nodo: condición de modo dispersión cíclico --------------------
InkyScatterMode::InkyScatterMode()
    : lastSwitch(std::chrono::high_resolution_clock::now()),
      scatterActive(true) {}

Status InkyScatterMode::update() {
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - lastSwitch;
    float cycleTime = fmod(elapsed.count(), 27.0f);
    scatterActive = (cycleTime < 7.0f);
    return scatterActive ? BH_SUCCESS : BH_FAILURE;
}

// -------------------- Nodo: Scatter (ir a esquina inferior derecha) --------------------
InkyScatter::InkyScatter() {
    // Esquina inferior derecha: asumimos un área de juego de ~800x600
    cornerTarget = std::make_pair(800, 600);
}

Status InkyScatter::update() {
    auto character = InkyInfo::getInfo()->in_character;
    auto gs = InkyInfo::getInfo()->in_gamestate;

    std::vector<Move> moves;
    if (character->getDirection() == PASS)
        moves = gs->getMaze().getPossibleMoves(character->getPos());
    else
        moves = gs->getMaze().getGhostLegalMoves(character->getPos(), character->getDirection());

    if (moves.empty()) {
        InkyInfo::getInfo()->out_move = PASS;
        return BH_FAILURE;
    }

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
    InkyInfo::getInfo()->out_move = bestMove;
    if (!nogui) std::cout << "Inky BT: Scatter - yendo a esquina inferior derecha" << std::endl;
    return BH_SUCCESS;
}

// -------------------- Nodo: Chase (persecución con intercepción usando Blinky) --------------------
Status InkyChase::update() {
    auto character = InkyInfo::getInfo()->in_character;
    auto gs = InkyInfo::getInfo()->in_gamestate;

    // 1. Posición y dirección de Pac-Man
    int pacmanNode = gs->getPacmanPos();
    auto pacmanPos = gs->getMaze().getNodePos(pacmanNode);
    Move pacmanDir = static_cast<Move>(gs->getPacmanDir());

    // 2. Punto 2 casillas (16 píxeles) por delante de Pac-Man
    int offsetX = 0, offsetY = 0;
    switch (pacmanDir) {
        case UP:    offsetY = -16; break;
        case DOWN:  offsetY =  16; break;
        case LEFT:  offsetX = -16; break;
        case RIGHT: offsetX =  16; break;
        default: break;
    }
    std::pair<int, int> pointAhead = {pacmanPos.first + offsetX, pacmanPos.second + offsetY};

    // 3. Posición de Blinky (asumimos índice 0)
    int blinkyNode = gs->getGhostsPos(0);
    auto blinkyPos = gs->getMaze().getNodePos(blinkyNode);

    // 4. Vector desde Blinky hacia pointAhead, duplicado: target = 2*pointAhead - blinkyPos
    std::pair<int, int> targetPos = {
        2 * pointAhead.first - blinkyPos.first,
        2 * pointAhead.second - blinkyPos.second
    };

    // 5. Obtener movimientos legales del fantasma
    std::vector<Move> moves;
    if (character->getDirection() == PASS)
        moves = gs->getMaze().getPossibleMoves(character->getPos());
    else
        moves = gs->getMaze().getGhostLegalMoves(character->getPos(), character->getDirection());

    if (moves.empty()) {
        InkyInfo::getInfo()->out_move = PASS;
        return BH_FAILURE;
    }

    // 6. Elegir movimiento que minimice distancia al objetivo
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
    InkyInfo::getInfo()->out_move = bestMove;
    if (!nogui) std::cout << "Inky BT: Chase - interceptando con Blinky" << std::endl;
    return BH_SUCCESS;
}