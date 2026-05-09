#include "PacmanBTController.h"
#include "Ghost.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

extern bool nogui;

PacmanInfo* PacmanInfo::instance = nullptr;


static std::vector<Move> getLegalMoves(std::shared_ptr<Character> ch, const GameState* gs) {
    return gs->getMaze().getPossibleMoves(ch->getPos());
}

// Constructor del arbol BT (Selector)
PacmanBTController::PacmanBTController(std::shared_ptr<Character> character)
    : Controller(character), root(std::make_shared<Selector>()) {

    // Prioridad alta: perseguir fantasmas comestibles
    auto chaseFilter = std::make_shared<Filter>();
    chaseFilter->addCondition(std::make_shared<NearbyEdibleGhost>());
    chaseFilter->addAction(std::make_shared<ChaseEdibleGhost>());
    root->addChild(chaseFilter);

    //  Huir de fantasmas peligrosos
    auto evadeFilter = std::make_shared<Filter>();
    evadeFilter->addCondition(std::make_shared<DangerousGhostNearby>(64.0f));
    evadeFilter->addAction(std::make_shared<EvadeGhosts>());
    root->addChild(evadeFilter);

    //  Recolectar comida cercana
    auto foodFilter = std::make_shared<Filter>();
    foodFilter->addCondition(std::make_shared<NearbyFood>(80.0f));
    foodFilter->addAction(std::make_shared<CollectFood>());
    root->addChild(foodFilter);

    // Comportamiento por defecto: exploracion aleatoria
    root->addChild(std::make_shared<RandomExplore>());
}

PacmanBTController::~PacmanBTController() {}

Move PacmanBTController::getMove(const GameState& gs) {
    PacmanInfo::getInfo()->in_character = character;
    PacmanInfo::getInfo()->in_gamestate = &gs;
    root->tick();
    return PacmanInfo::getInfo()->out_move;
}

// --------------------------- Implementación de nodos ---------------------------

Status NearbyEdibleGhost::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(gs->getPacmanPos());
    for (int i = 0; i < 4; ++i) {
        if (gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dx = ghostPos.first - pacmanPos.first;
            float dy = ghostPos.second - pacmanPos.second;
            if (dx*dx + dy*dy < 120*120) return BH_SUCCESS;
        }
    }
    return BH_FAILURE;
}

Status ChaseEdibleGhost::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanCoords = gs->getMaze().getNodePos(character->getPos());

    int targetNode = -1;
    float minDistSq = std::numeric_limits<float>::max();
    for (int i = 0; i < 4; ++i) {
        if (gs->isGhostEdible(i)) {
            auto ghostCoords = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dx = ghostCoords.first - pacmanCoords.first;
            float dy = ghostCoords.second - pacmanCoords.second;
            float distSq = dx*dx + dy*dy;
            if (distSq < minDistSq) {
                minDistSq = distSq;
                targetNode = gs->getGhostsPos(i);
            }
        }
    }
    if (targetNode == -1) return BH_FAILURE;

    if (!nogui) std::cout << "Pacman BT: Persiguiendo fantasma comestible." << std::endl;

    auto targetPos = gs->getMaze().getNodePos(targetNode);
    auto moves = getLegalMoves(character, gs);
    Move bestMove = PASS;
    float bestDist = std::numeric_limits<float>::max();
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
    PacmanInfo::getInfo()->out_move = bestMove;
    return BH_SUCCESS;
}

DangerousGhostNearby::DangerousGhostNearby(float distance) : threatDistance(distance) {}

Status DangerousGhostNearby::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanCoords = gs->getMaze().getNodePos(gs->getPacmanPos());
    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            auto ghostCoords = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dx = ghostCoords.first - pacmanCoords.first;
            float dy = ghostCoords.second - pacmanCoords.second;
            if (dx*dx + dy*dy < threatDistance * threatDistance) return BH_SUCCESS;
        }
    }
    return BH_FAILURE;
}

Status EvadeGhosts::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanCoords = gs->getMaze().getNodePos(character->getPos());

    float minDistSq = std::numeric_limits<float>::max();
    int closestGhostNode = -1;
    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            auto ghostCoords = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dx = ghostCoords.first - pacmanCoords.first;
            float dy = ghostCoords.second - pacmanCoords.second;
            float distSq = dx*dx + dy*dy;
            if (distSq < minDistSq) {
                minDistSq = distSq;
                closestGhostNode = gs->getGhostsPos(i);
            }
        }
    }
    if (closestGhostNode == -1) return BH_FAILURE;

    if (!nogui) std::cout << "Pacman BT: Huyendo de fantasma peligroso." << std::endl;

    auto ghostPos = gs->getMaze().getNodePos(closestGhostNode);
    auto moves = getLegalMoves(character, gs);
    Move bestMove = PASS;
    float maxDist = -1.0f;
    for (Move m : moves) {
        if (m == PASS) continue;
        int neighbor = gs->getMaze().getNeighbour(character->getPos(), m);
        auto pos = gs->getMaze().getNodePos(neighbor);
        float dx = pos.first - ghostPos.first;
        float dy = pos.second - ghostPos.second;
        float distSq = dx*dx + dy*dy;
        if (distSq > maxDist) {
            maxDist = distSq;
            bestMove = m;
        }
    }
    PacmanInfo::getInfo()->out_move = bestMove;
    return BH_SUCCESS;
}

NearbyFood::NearbyFood(float radius) : searchRadius(radius) {}

Status NearbyFood::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanCoords = gs->getMaze().getNodePos(gs->getPacmanPos());

    // Comprobar píldoras
    for (const auto& pillPos : gs->getMaze().getPillPositions()) {
        float dx = pillPos.first - pacmanCoords.first;
        float dy = pillPos.second - pacmanCoords.second;
        if (dx*dx + dy*dy < searchRadius * searchRadius) return BH_SUCCESS;
    }
    // Comprobar powerpills
    for (const auto& ppPos : gs->getMaze().getPowerPillPositions()) {
        float dx = ppPos.first - pacmanCoords.first;
        float dy = ppPos.second - pacmanCoords.second;
        if (dx*dx + dy*dy < searchRadius * searchRadius) return BH_SUCCESS;
    }
    return BH_FAILURE;
}

Status CollectFood::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanCoords = gs->getMaze().getNodePos(character->getPos());

    int bestNode = -1;
    float bestDistSq = std::numeric_limits<float>::max();

    // Recorrer un rango suficientemente grande (maximo de nodos razonable)
    for (int node = 0; node < 500; ++node) {
        if (gs->getMaze().hasPill(node) || gs->getMaze().hasPowerPill(node)) {
            auto foodPos = gs->getMaze().getNodePos(node);
            float dx = foodPos.first - pacmanCoords.first;
            float dy = foodPos.second - pacmanCoords.second;
            float distSq = dx*dx + dy*dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestNode = node;
            }
        }
    }

    if (bestNode == -1) return BH_FAILURE;

    if (!nogui) std::cout << "Pacman BT: Recolectando comida cercana." << std::endl;

    auto targetPos = gs->getMaze().getNodePos(bestNode);
    auto moves = getLegalMoves(character, gs);
    Move bestMove = PASS;
    float minDist = std::numeric_limits<float>::max();
    for (Move m : moves) {
        if (m == PASS) continue;
        int neighbor = gs->getMaze().getNeighbour(character->getPos(), m);
        auto pos = gs->getMaze().getNodePos(neighbor);
        float dx = pos.first - targetPos.first;
        float dy = pos.second - targetPos.second;
        float distSq = dx*dx + dy*dy;
        if (distSq < minDist) {
            minDist = distSq;
            bestMove = m;
        }
    }
    PacmanInfo::getInfo()->out_move = bestMove;
    return BH_SUCCESS;
}

RandomExplore::RandomExplore()
    : rng(std::chrono::steady_clock::now().time_since_epoch().count()),
      dist(0, 3) {}

Status RandomExplore::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto moves = getLegalMoves(character, gs);
    if (moves.empty()) {
        PacmanInfo::getInfo()->out_move = PASS;
        return BH_FAILURE;
    }
    int idx = dist(rng) % moves.size();
    if (!nogui) std::cout << "Pacman BT: Explorando aleatoriamente." << std::endl;
    PacmanInfo::getInfo()->out_move = moves[idx];
    return BH_SUCCESS;
}