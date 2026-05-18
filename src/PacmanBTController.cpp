#include "PacmanBTController.h"
#include "Ghost.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

extern bool nogui;

PacmanInfo* PacmanInfo::instance = nullptr;

// Helper: movimientos legales
static std::vector<Move> getLegalMoves(std::shared_ptr<Character> ch, const GameState* gs) {
    return gs->getMaze().getPossibleMoves(ch->getPos());
}

// Helper: distancia de Manhattan
static float manhattanDistance(const std::pair<int,int>& a, const std::pair<int,int>& b) {
    return std::abs(a.first - b.first) + std::abs(a.second - b.second);
}

// Helper: encuentra comida más cercana (usa cache)
static int findNearestFood(const std::pair<int,int>& fromPos, const GameState* gs, float* outDist = nullptr) {
    auto* info = PacmanInfo::getInfo();  // CORREGIDO: auto* en lugar de auto&
    
    if (!info->food_cache_valid) {
        info->cached_food_nodes.clear();
        for (int node = 0; node < 500; ++node) {
            if (gs->getMaze().hasPill(node) || gs->getMaze().hasPowerPill(node)) {
                info->cached_food_nodes.push_back(node);
            }
        }
        info->food_cache_valid = true;
    }

    int bestNode = -1;
    float bestDist = std::numeric_limits<float>::max();
    
    for (int node : info->cached_food_nodes) {
        auto foodPos = gs->getMaze().getNodePos(node);
        float dist = manhattanDistance(fromPos, foodPos);
        if (dist < bestDist) {
            bestDist = dist;
            bestNode = node;
        }
    }
    
    if (outDist) *outDist = bestDist;
    return bestNode;
}

// Helper mejorado: selecciona mejor movimiento con ANTI-OSCILACIÓN
static Move selectBestMove(std::shared_ptr<Character> ch, const GameState* gs,
                          const std::pair<int,int>& target, bool minimize) {
    auto* info = PacmanInfo::getInfo();  // CORREGIDO: auto* en lugar de auto&
    std::vector<Move> rawMoves = getLegalMoves(ch, gs);
    if (rawMoves.empty()) return PASS;

    // ANTI-OSCILACIÓN: Si tenemos un compromiso activo, evaluarlo primero
    if (info->commitment_ticks > 0) {
        info->commitment_ticks--;
        if (std::find(rawMoves.begin(), rawMoves.end(), info->committed_move) != rawMoves.end()) {
            return info->committed_move;
        }
        // Si el compromiso ya no es válido, resetear
        info->commitment_ticks = 0;
    }

    struct MoveScore {
        Move move;
        float dist;
    };
    std::vector<MoveScore> scores;
    
    for (Move m : rawMoves) {
        if (m == PASS) continue;
        int neighbor = gs->getMaze().getNeighbour(ch->getPos(), m);
        auto pos = gs->getMaze().getNodePos(neighbor);
        float dist = manhattanDistance(pos, target);
        scores.push_back({m, dist});
    }
    
    if (scores.empty()) return rawMoves[0];

    // Ordenar
    if (minimize) {
        std::sort(scores.begin(), scores.end(),
                  [](const MoveScore& a, const MoveScore& b) { return a.dist < b.dist; });
    } else {
        std::sort(scores.begin(), scores.end(),
                  [](const MoveScore& a, const MoveScore& b) { return a.dist > b.dist; });
    }

    Move bestMove = scores[0].move;
    Move currentDir = ch->getDirection();
    
    // INERCIA con hysteresis: mantener dirección actual si es competitiva
    if (currentDir != PASS) {
        for (size_t i = 0; i < std::min((size_t)2, scores.size()); ++i) {
            if (scores[i].move == currentDir) {
                // Solo cambiar si el nuevo movimiento es SIGNIFICATIVAMENTE mejor (>15%)
                if (i > 0 && scores[0].dist < scores[i].dist * 0.85f) {
                    bestMove = scores[0].move;
                } else {
                    bestMove = currentDir;
                }
                break;
            }
        }
    }
    
    // Crear compromiso para prevenir oscilaciones (3-5 ticks)
    info->committed_move = bestMove;
    info->commitment_ticks = 3;
    
    return bestMove;
}

// --------------------------- Constructor del árbol BT ---------------------------
PacmanBTController::PacmanBTController(std::shared_ptr<Character> character)
    : Controller(character), root(std::make_shared<Selector>()) {

    // 1. PERSEGUIR FANTASMA COMESTIBLE solo si vale la pena
    auto chaseFilter = std::make_shared<Filter>();
    chaseFilter->addCondition(std::make_shared<WorthChasingGhost>());
    chaseFilter->addAction(std::make_shared<ChaseNearbyGhost>());
    root->addChild(chaseFilter);

    // 2. PELIGRO INMEDIATO: huir urgentemente
    auto emergencyFilter = std::make_shared<Filter>();
    emergencyFilter->addCondition(std::make_shared<ImmediateDanger>());
    emergencyFilter->addAction(std::make_shared<EmergencyEvade>());
    root->addChild(emergencyFilter);

    // 3. POWER PILL ESTRATÉGICA: solo si realmente vale la pena
    auto powerPillFilter = std::make_shared<Filter>();
    powerPillFilter->addCondition(std::make_shared<WorthGettingPowerPill>());
    powerPillFilter->addAction(std::make_shared<GetStrategicPowerPill>());
    root->addChild(powerPillFilter);

    // 4. OBJETIVO PRINCIPAL: recolectar comida
    auto foodFilter = std::make_shared<Filter>();
    foodFilter->addCondition(std::make_shared<FoodRemaining>());
    foodFilter->addAction(std::make_shared<CollectNearestFood>());
    root->addChild(foodFilter);

    // 5. FALLBACK: exploración con compromiso
    root->addChild(std::make_shared<CommittedExplore>());
}

PacmanBTController::~PacmanBTController() {}

Move PacmanBTController::getMove(const GameState& gs) {
    auto* info = PacmanInfo::getInfo();  // CORREGIDO: auto* en lugar de auto&
    info->in_character = character;
    info->in_gamestate = &gs;
    info->food_cache_valid = false;
    
    // Detectar si estamos atascados (misma posición)
    int currentPos = character->getPos();
    if (currentPos == info->last_position) {
        // Resetear compromiso si estamos atascados
        info->commitment_ticks = 0;
    }
    info->last_position = currentPos;
    
    root->tick();
    return info->out_move;
}

// --------------------------- CONDICIONES ---------------------------

Status WorthChasingGhost::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(gs->getPacmanPos());

    int closestGhostNode = -1;
    float closestGhostDist = std::numeric_limits<float>::max();
    
    // Encontrar fantasma comestible más cercano
    for (int i = 0; i < 4; ++i) {
        if (gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist = manhattanDistance(pacmanPos, ghostPos);
            if (dist < closestGhostDist) {
                closestGhostDist = dist;
                closestGhostNode = gs->getGhostsPos(i);
            }
        }
    }
    
    if (closestGhostNode == -1) return BH_FAILURE;
    
    // Encontrar comida más cercana
    float closestFoodDist;
    int foodNode = findNearestFood(pacmanPos, gs, &closestFoodDist);
    
    // LÓGICA MEJORADA: perseguir fantasma solo si:
    // 1. El fantasma está MUY cerca (< 30 unidades) O
    // 2. El fantasma está más cerca que la comida
    bool veryClose = closestGhostDist < 30.0f;
    bool closerThanFood = (foodNode == -1) || (closestGhostDist < closestFoodDist * 0.8f);
    
    if (veryClose || closerThanFood) {
        if (!nogui) std::cout << "Vale la pena perseguir fantasma (ghost: " << closestGhostDist 
                              << ", food: " << closestFoodDist << ")" << std::endl;
        return BH_SUCCESS;
    }
    
    return BH_FAILURE;
}

ImmediateDanger::ImmediateDanger(float distance) : criticalDistance(distance) {}

Status ImmediateDanger::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(gs->getPacmanPos());
    
    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist = manhattanDistance(pacmanPos, ghostPos);
            if (dist < criticalDistance) {
                if (!nogui) std::cout << "¡PELIGRO INMEDIATO! Fantasma a " << dist << std::endl;
                return BH_SUCCESS;
            }
        }
    }
    return BH_FAILURE;
}

Status WorthGettingPowerPill::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(gs->getPacmanPos());

    // 1. Verificar si hay fantasmas peligrosos cercanos (< 70 unidades)
    float closestThreatDist = std::numeric_limits<float>::max();
    int threatCount = 0;
    
    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist = manhattanDistance(pacmanPos, ghostPos);
            if (dist < 70.0f) {
                threatCount++;
                closestThreatDist = std::min(closestThreatDist, dist);
            }
        }
    }
    
    // Solo considerar si hay amenaza real
    if (threatCount == 0) return BH_FAILURE;

    // 2. Encontrar Power Pill más cercana
    int closestPP = -1;
    float closestPPDist = std::numeric_limits<float>::max();
    
    for (int node = 0; node < 500; ++node) {
        if (gs->getMaze().hasPowerPill(node)) {
            auto ppPos = gs->getMaze().getNodePos(node);
            float dist = manhattanDistance(pacmanPos, ppPos);
            if (dist < closestPPDist) {
                closestPPDist = dist;
                closestPP = node;
            }
        }
    }
    
    if (closestPP == -1) return BH_FAILURE;

    // 3. Encontrar comida más cercana para comparar
    float closestFoodDist;
    findNearestFood(pacmanPos, gs, &closestFoodDist);

    // LÓGICA MEJORADA: ir por Power Pill solo si:
    // - Hay amenaza cercana (< 50 unidades) Y
    // - La Power Pill está más cerca que huir (no demasiado lejos) Y
    // - (Power Pill está más cerca que comida O amenaza es muy cercana)
    bool closeEnoughThreat = closestThreatDist < 50.0f;
    bool ppAccessible = closestPPDist < 60.0f;
    bool worthIt = (closestPPDist < closestFoodDist) || (closestThreatDist < 40.0f);
    
    if (closeEnoughThreat && ppAccessible && worthIt) {
        if (!nogui) std::cout << "Vale la pena ir por Power Pill (threat: " << closestThreatDist 
                              << ", pp: " << closestPPDist << ", food: " << closestFoodDist << ")" << std::endl;
        return BH_SUCCESS;
    }
    
    return BH_FAILURE;
}

Status FoodRemaining::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    for (int node = 0; node < 500; ++node) {
        if (gs->getMaze().hasPill(node) || gs->getMaze().hasPowerPill(node)) {
            return BH_SUCCESS;
        }
    }
    return BH_FAILURE;
}

// --------------------------- ACCIONES ---------------------------

Status ChaseNearbyGhost::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(character->getPos());

    int targetNode = -1;
    float minDist = std::numeric_limits<float>::max();
    
    for (int i = 0; i < 4; ++i) {
        if (gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist = manhattanDistance(pacmanPos, ghostPos);
            if (dist < minDist) {
                minDist = dist;
                targetNode = gs->getGhostsPos(i);
            }
        }
    }
    
    if (targetNode == -1) return BH_FAILURE;

    if (!nogui) std::cout << "→ Persiguiendo fantasma comestible (dist: " << minDist << ")" << std::endl;

    auto targetPos = gs->getMaze().getNodePos(targetNode);
    Move move = selectBestMove(character, gs, targetPos, true);
    PacmanInfo::getInfo()->out_move = move;
    return BH_SUCCESS;
}

Status EmergencyEvade::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(character->getPos());

    int closestThreat = -1;
    float minDist = std::numeric_limits<float>::max();
    
    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist = manhattanDistance(pacmanPos, ghostPos);
            if (dist < minDist) {
                minDist = dist;
                closestThreat = gs->getGhostsPos(i);
            }
        }
    }
    
    if (closestThreat == -1) return BH_FAILURE;

    if (!nogui) std::cout << "→ ¡HUYENDO! (dist: " << minDist << ")" << std::endl;

    auto threatPos = gs->getMaze().getNodePos(closestThreat);
    Move move = selectBestMove(character, gs, threatPos, false); // maximizar distancia
    PacmanInfo::getInfo()->out_move = move;
    return BH_SUCCESS;
}

Status GetStrategicPowerPill::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(character->getPos());

    int bestPP = -1;
    float bestDist = std::numeric_limits<float>::max();
    
    for (int node = 0; node < 500; ++node) {
        if (gs->getMaze().hasPowerPill(node)) {
            auto ppPos = gs->getMaze().getNodePos(node);
            float dist = manhattanDistance(pacmanPos, ppPos);
            if (dist < bestDist) {
                bestDist = dist;
                bestPP = node;
            }
        }
    }
    
    if (bestPP == -1) return BH_FAILURE;

    if (!nogui) std::cout << "→ Buscando Power Pill (dist: " << bestDist << ")" << std::endl;

    auto targetPos = gs->getMaze().getNodePos(bestPP);
    Move move = selectBestMove(character, gs, targetPos, true);
    PacmanInfo::getInfo()->out_move = move;
    return BH_SUCCESS;
}

Status CollectNearestFood::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto pacmanPos = gs->getMaze().getNodePos(character->getPos());

    float foodDist;
    int foodNode = findNearestFood(pacmanPos, gs, &foodDist);
    
    if (foodNode == -1) return BH_FAILURE;

    if (!nogui) std::cout << "→ Recolectando comida (dist: " << foodDist << ")" << std::endl;

    auto targetPos = gs->getMaze().getNodePos(foodNode);
    Move move = selectBestMove(character, gs, targetPos, true);
    PacmanInfo::getInfo()->out_move = move;
    return BH_SUCCESS;
}

CommittedExplore::CommittedExplore()
    : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

Status CommittedExplore::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    auto* info = PacmanInfo::getInfo();  // CORREGIDO: auto* en lugar de auto&
    
    std::vector<Move> moves = getLegalMoves(character, gs);
    if (moves.empty()) {
        info->out_move = PASS;
        return BH_FAILURE;
    }

    // Si tenemos compromiso activo, usarlo
    if (info->commitment_ticks > 0) {
        info->commitment_ticks--;
        if (std::find(moves.begin(), moves.end(), info->committed_move) != moves.end()) {
            info->out_move = info->committed_move;
            if (!nogui) std::cout << "→ Explorando (comprometido)" << std::endl;
            return BH_SUCCESS;
        }
    }

    // Nuevo movimiento exploratorio
    Move currentDir = character->getDirection();
    
    // 70% probabilidad de continuar en la misma dirección
    if (std::find(moves.begin(), moves.end(), currentDir) != moves.end()) {
        std::uniform_int_distribution<int> dist(0, 99);
        if (dist(rng) < 70) {
            info->out_move = currentDir;
            info->committed_move = currentDir;
            info->commitment_ticks = 5; // compromiso más largo en exploración
            if (!nogui) std::cout << "→ Explorando (continuando)" << std::endl;
            return BH_SUCCESS;
        }
    }
    
    // Elegir nueva dirección aleatoria
    std::uniform_int_distribution<int> dist(0, moves.size() - 1);
    Move newMove = moves[dist(rng)];
    info->out_move = newMove;
    info->committed_move = newMove;
    info->commitment_ticks = 5;
    
    if (!nogui) std::cout << "→ Explorando (nueva dirección)" << std::endl;
    return BH_SUCCESS;
}