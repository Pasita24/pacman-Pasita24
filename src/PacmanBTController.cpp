#include "PacmanBTController.h"
#include "Ghost.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

extern bool nogui;

PacmanInfo* PacmanInfo::instance = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<Move> getLegalMoves(std::shared_ptr<Character> ch,
                                       const GameState* gs) {
    return gs->getMaze().getPossibleMoves(ch->getPos());
}

static float manhattanDistance(const std::pair<int,int>& a,
                               const std::pair<int,int>& b) {
    return std::abs(a.first - b.first) + std::abs(a.second - b.second);
}

// Cuenta movimientos posibles desde un nodo dado (grado del nodo)
static int countMoves(int node, const GameState* gs) {
    int count = 0;
    const Move dirs[] = { UP, DOWN, LEFT, RIGHT };
    for (Move m : dirs) {
        int nb = gs->getMaze().getNeighbour(node, m);
        if (nb != node && nb >= 0) ++count;
    }
    return count;
}

// Devuelve true si el nodo es una intersección real (>= 3 salidas)
static bool isIntersection(int node, const GameState* gs) {
    return countMoves(node, gs) >= 3;
}

// Encuentra comida más cercana usando cache
static int findNearestFood(const std::pair<int,int>& fromPos,
                           const GameState* gs, float* outDist = nullptr) {
    auto* info = PacmanInfo::getInfo();
    if (!info->food_cache_valid) {
        info->cached_food_nodes.clear();
        for (int node = 0; node < 500; ++node) {
            if (gs->getMaze().hasPill(node) || gs->getMaze().hasPowerPill(node))
                info->cached_food_nodes.push_back(node);
        }
        info->food_cache_valid = true;
    }

    int bestNode = -1;
    float bestDist = std::numeric_limits<float>::max();
    for (int node : info->cached_food_nodes) {
        float dist = manhattanDistance(fromPos, gs->getMaze().getNodePos(node));
        if (dist < bestDist) { bestDist = dist; bestNode = node; }
    }
    if (outDist) *outDist = bestDist;
    return bestNode;
}

static float ghostDangerAt(int destNode, const GameState* gs,
                            float decayRadius = 40.0f) {
    auto destPos = gs->getMaze().getNodePos(destNode);
    float totalDanger = 0.0f;
    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist = manhattanDistance(destPos, ghostPos);
            // Peligro inversamente proporcional a la distancia
            if (dist < decayRadius) {
                totalDanger += (decayRadius - dist) / decayRadius;
            }
        }
    }
    return totalDanger;
}


static float fleeScoreAt(int destNode, const GameState* gs) {
    auto destPos = gs->getMaze().getNodePos(destNode);
    float minDist = std::numeric_limits<float>::max();
    float totalDist = 0.0f;
    int threatCount = 0;

    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist = manhattanDistance(destPos, ghostPos);
            minDist = std::min(minDist, dist);
            totalDist += dist;
            ++threatCount;
        }
    }
    if (threatCount == 0) return 0.0f;
    // Ponderamos: 60% distancia mínima (huir del más cercano)
    //             40% distancia total  (no correr hacia otros)
    float avgDist = totalDist / threatCount;
    return 0.6f * minDist + 0.4f * avgDist;
}

static Move selectBestMove(std::shared_ptr<Character> ch, const GameState* gs,
                           const std::pair<int,int>& target, bool minimize,
                           bool flee = false) {
    auto* info = PacmanInfo::getInfo();
    std::vector<Move> rawMoves = getLegalMoves(ch, gs);
    if (rawMoves.empty()) return PASS;

    int currentNode = ch->getPos();

    // Si estamos en una intersección, rompemos el commitment para decidir bien
    if (isIntersection(currentNode, gs)) {
        info->commitment_ticks = 0;
    }

    // Si hay commitment activo y el movimiento sigue siendo legal, usarlo
    if (info->commitment_ticks > 0) {
        info->commitment_ticks--;
        if (std::find(rawMoves.begin(), rawMoves.end(),
                      info->committed_move) != rawMoves.end()) {
            return info->committed_move;
        }
        info->commitment_ticks = 0;
    }

    struct MoveScore {
        Move move;
        float score; // menor = mejor cuando minimize, mayor = mejor cuando flee
    };
    std::vector<MoveScore> scores;

    for (Move m : rawMoves) {
        if (m == PASS) continue;
        int neighbor = gs->getMaze().getNeighbour(currentNode, m);
        if (neighbor < 0 || neighbor == currentNode) continue;

        auto neighborPos = gs->getMaze().getNodePos(neighbor);
        float baseScore;

        if (flee) {
            // Modo huida: queremos maximizar el flee score
            // Lo convertimos en "costo a minimizar" negándolo
            baseScore = -fleeScoreAt(neighbor, gs);
        } else {
            baseScore = manhattanDistance(neighborPos, target);
        }

        // Penalizar callejones sin salida: si el destino tiene solo 1 salida
        // (y no es nuestro objetivo inmediato), añadir penalidad
        int exits = countMoves(neighbor, gs);
        if (exits <= 1) {
            baseScore += flee ? -20.0f : 20.0f; // callejón es malo en ambos modos
        }

        // Penalizar por peligro fantasmal en el nodo destino
        // (útil en modo comida/power pill: no caminar hacia un fantasma)
        if (!flee) {
            float danger = ghostDangerAt(neighbor, gs, 30.0f);
            baseScore += danger * 15.0f;
        }

        scores.push_back({m, baseScore});
    }

    if (scores.empty()) return rawMoves[0];

    // Siempre minimizamos `score` (flee ya lo negó para que funcione igual)
    std::sort(scores.begin(), scores.end(),
              [](const MoveScore& a, const MoveScore& b) {
                  return a.score < b.score;
              });

    Move bestMove = scores[0].move;
    Move currentDir = ch->getDirection();

    // Inercia con hysteresis: mantener si es competitivo (< 15% peor)
    if (currentDir != PASS) {
        for (size_t i = 0; i < std::min((size_t)2, scores.size()); ++i) {
            if (scores[i].move == currentDir) {
                if (i == 0 || scores[0].score < scores[i].score * 0.85f) {
                    bestMove = (i == 0) ? currentDir : scores[0].move;
                } else {
                    bestMove = currentDir;
                }
                break;
            }
        }
    }

    // Crear compromiso solo si NO estamos en una intersección
    // (en intersecciones decidimos en cada tick)
    if (!isIntersection(currentNode, gs)) {
        info->committed_move = bestMove;
        info->commitment_ticks = 3;
    }

    return bestMove;
}

// ---------------------------------------------------------------------------
// Constructor del BT
// ---------------------------------------------------------------------------
PacmanBTController::PacmanBTController(std::shared_ptr<Character> character)
    : Controller(character), root(std::make_shared<Selector>()) {

    // 1. Perseguir fantasma comestible (si vale la pena)
    auto chaseFilter = std::make_shared<Filter>();
    chaseFilter->addCondition(std::make_shared<WorthChasingGhost>());
    chaseFilter->addAction(std::make_shared<ChaseNearbyGhost>());
    root->addChild(chaseFilter);

    // 2. Peligro inmediato: huir con evasión multi-fantasma
    auto emergencyFilter = std::make_shared<Filter>();
    emergencyFilter->addCondition(std::make_shared<ImmediateDanger>());
    emergencyFilter->addAction(std::make_shared<MultiGhostEvade>());
    root->addChild(emergencyFilter);

    // 3. Power Pill estratégica
    auto powerPillFilter = std::make_shared<Filter>();
    powerPillFilter->addCondition(std::make_shared<WorthGettingPowerPill>());
    powerPillFilter->addAction(std::make_shared<GetStrategicPowerPill>());
    root->addChild(powerPillFilter);

    // 4. Recolectar comida
    auto foodFilter = std::make_shared<Filter>();
    foodFilter->addCondition(std::make_shared<FoodRemaining>());
    foodFilter->addAction(std::make_shared<CollectNearestFood>());
    root->addChild(foodFilter);

    // 5. Exploración fallback
    root->addChild(std::make_shared<CommittedExplore>());
}

PacmanBTController::~PacmanBTController() {}

Move PacmanBTController::getMove(const GameState& gs) {
    auto* info = PacmanInfo::getInfo();
    info->in_character = character;
    info->in_gamestate = &gs;
    info->food_cache_valid = false;

    int currentPos = character->getPos();
    if (currentPos == info->last_position) {
        // Atascados: romper compromiso
        info->commitment_ticks = 0;
    }
    info->last_position = currentPos;

    root->tick();
    return info->out_move;
}

// ---------------------------------------------------------------------------
// CONDICIONES
// ---------------------------------------------------------------------------

Status WorthChasingGhost::update() {
    auto gs  = PacmanInfo::getInfo()->in_gamestate;
    auto pac = gs->getMaze().getNodePos(gs->getPacmanPos());

    float closestGhostDist = std::numeric_limits<float>::max();
    bool  anyEdible = false;

    for (int i = 0; i < 4; ++i) {
        if (gs->isGhostEdible(i)) {
            anyEdible = true;
            float dist = manhattanDistance(pac,
                            gs->getMaze().getNodePos(gs->getGhostsPos(i)));
            closestGhostDist = std::min(closestGhostDist, dist);
        }
    }
    if (!anyEdible) return BH_FAILURE;

    float foodDist;
    int   foodNode = findNearestFood(pac, gs, &foodDist);

    bool veryClose      = closestGhostDist < 30.0f;
    bool closerThanFood = (foodNode == -1) ||
                          (closestGhostDist < foodDist * 0.8f);

    if (veryClose || closerThanFood) {
        if (!nogui)
            std::cout << "Vale la pena perseguir fantasma (ghost: "
                      << closestGhostDist << ", food: " << foodDist << ")\n";
        return BH_SUCCESS;
    }
    return BH_FAILURE;
}

ImmediateDanger::ImmediateDanger(float distance) : criticalDistance(distance) {}

Status ImmediateDanger::update() {
    auto gs  = PacmanInfo::getInfo()->in_gamestate;
    auto pac = gs->getMaze().getNodePos(gs->getPacmanPos());

    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            float dist = manhattanDistance(pac,
                            gs->getMaze().getNodePos(gs->getGhostsPos(i)));
            if (dist < criticalDistance) {
                if (!nogui)
                    std::cout << "¡PELIGRO INMEDIATO! Fantasma a " << dist << "\n";
                return BH_SUCCESS;
            }
        }
    }
    return BH_FAILURE;
}

Status WorthGettingPowerPill::update() {
    auto gs  = PacmanInfo::getInfo()->in_gamestate;
    auto pac = gs->getMaze().getNodePos(gs->getPacmanPos());

    float closestThreatDist = std::numeric_limits<float>::max();
    int   threatCount = 0;

    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            float dist = manhattanDistance(pac,
                            gs->getMaze().getNodePos(gs->getGhostsPos(i)));
            if (dist < 70.0f) {
                ++threatCount;
                closestThreatDist = std::min(closestThreatDist, dist);
            }
        }
    }
    if (threatCount == 0) return BH_FAILURE;

    int   closestPP   = -1;
    float closestPPDist = std::numeric_limits<float>::max();
    for (int node = 0; node < 500; ++node) {
        if (gs->getMaze().hasPowerPill(node)) {
            float dist = manhattanDistance(pac,
                            gs->getMaze().getNodePos(node));
            if (dist < closestPPDist) { closestPPDist = dist; closestPP = node; }
        }
    }
    if (closestPP == -1) return BH_FAILURE;

    float foodDist;
    findNearestFood(pac, gs, &foodDist);

    bool closeEnoughThreat = closestThreatDist < 50.0f;
    bool ppAccessible      = closestPPDist < 60.0f;
    bool worthIt           = (closestPPDist < foodDist) ||
                             (closestThreatDist < 40.0f);

    if (closeEnoughThreat && ppAccessible && worthIt) {
        if (!nogui)
            std::cout << "Vale la pena ir por Power Pill (threat: "
                      << closestThreatDist << ", pp: " << closestPPDist
                      << ", food: " << foodDist << ")\n";
        return BH_SUCCESS;
    }
    return BH_FAILURE;
}

Status FoodRemaining::update() {
    auto gs = PacmanInfo::getInfo()->in_gamestate;
    for (int node = 0; node < 500; ++node) {
        if (gs->getMaze().hasPill(node) || gs->getMaze().hasPowerPill(node))
            return BH_SUCCESS;
    }
    return BH_FAILURE;
}

// ---------------------------------------------------------------------------
// ACCIONES
// ---------------------------------------------------------------------------

Status ChaseNearbyGhost::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs        = PacmanInfo::getInfo()->in_gamestate;
    auto pac       = gs->getMaze().getNodePos(character->getPos());

    int   targetNode = -1;
    float minDist    = std::numeric_limits<float>::max();

    for (int i = 0; i < 4; ++i) {
        if (gs->isGhostEdible(i)) {
            auto ghostPos = gs->getMaze().getNodePos(gs->getGhostsPos(i));
            float dist    = manhattanDistance(pac, ghostPos);
            if (dist < minDist) { minDist = dist; targetNode = gs->getGhostsPos(i); }
        }
    }
    if (targetNode == -1) return BH_FAILURE;

    if (!nogui)
        std::cout << "→ Persiguiendo fantasma comestible (dist: " << minDist << ")\n";

    auto targetPos = gs->getMaze().getNodePos(targetNode);
    PacmanInfo::getInfo()->out_move =
        selectBestMove(character, gs, targetPos, true, false);
    return BH_SUCCESS;
}

// ---------------------------------------------------------------------------
Status MultiGhostEvade::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs        = PacmanInfo::getInfo()->in_gamestate;
    auto pac       = gs->getMaze().getNodePos(character->getPos());

    // Verificar que hay alguna amenaza real
    bool anyThreat = false;
    float closestDist = std::numeric_limits<float>::max();
    for (int i = 0; i < 4; ++i) {
        if (!gs->isGhostEdible(i)) {
            float dist = manhattanDistance(pac,
                            gs->getMaze().getNodePos(gs->getGhostsPos(i)));
            closestDist = std::min(closestDist, dist);
            anyThreat = true;
        }
    }
    if (!anyThreat) return BH_FAILURE;

    if (!nogui)
        std::cout << "→ ¡HUYENDO MULTI-GHOST! (más cercano: "
                  << closestDist << ")\n";

    // target ignorado en modo flee — pasamos {0,0} como placeholder
    PacmanInfo::getInfo()->out_move =
        selectBestMove(character, gs, {0, 0}, false, true);
    return BH_SUCCESS;
}

Status GetStrategicPowerPill::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs        = PacmanInfo::getInfo()->in_gamestate;
    auto pac       = gs->getMaze().getNodePos(character->getPos());

    int   bestPP   = -1;
    float bestDist = std::numeric_limits<float>::max();

    for (int node = 0; node < 500; ++node) {
        if (gs->getMaze().hasPowerPill(node)) {
            float dist = manhattanDistance(pac, gs->getMaze().getNodePos(node));
            if (dist < bestDist) { bestDist = dist; bestPP = node; }
        }
    }
    if (bestPP == -1) return BH_FAILURE;

    if (!nogui)
        std::cout << "→ Buscando Power Pill (dist: " << bestDist << ")\n";

    auto targetPos = gs->getMaze().getNodePos(bestPP);
    PacmanInfo::getInfo()->out_move =
        selectBestMove(character, gs, targetPos, true, false);
    return BH_SUCCESS;
}

Status CollectNearestFood::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs        = PacmanInfo::getInfo()->in_gamestate;
    auto pac       = gs->getMaze().getNodePos(character->getPos());

    float foodDist;
    int   foodNode = findNearestFood(pac, gs, &foodDist);
    if (foodNode == -1) return BH_FAILURE;

    if (!nogui)
        std::cout << "→ Recolectando comida (dist: " << foodDist << ")\n";

    auto targetPos = gs->getMaze().getNodePos(foodNode);
    PacmanInfo::getInfo()->out_move =
        selectBestMove(character, gs, targetPos, true, false);
    return BH_SUCCESS;
}

CommittedExplore::CommittedExplore()
    : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

Status CommittedExplore::update() {
    auto character = PacmanInfo::getInfo()->in_character;
    auto gs        = PacmanInfo::getInfo()->in_gamestate;
    auto* info     = PacmanInfo::getInfo();

    std::vector<Move> moves = getLegalMoves(character, gs);
    if (moves.empty()) { info->out_move = PASS; return BH_FAILURE; }

    int currentNode = character->getPos();

    // En intersecciones: siempre decidir de nuevo (no reutilizar commitment)
    bool atIntersection = isIntersection(currentNode, gs);
    if (atIntersection) info->commitment_ticks = 0;

    // Reutilizar commitment si sigue siendo válido
    if (info->commitment_ticks > 0) {
        info->commitment_ticks--;
        if (std::find(moves.begin(), moves.end(),
                      info->committed_move) != moves.end()) {
            info->out_move = info->committed_move;
            if (!nogui) std::cout << "→ Explorando (comprometido)\n";
            return BH_SUCCESS;
        }
    }

    Move currentDir = character->getDirection();

    // En corredor recto (2 salidas): 80% continuar en misma dirección
    if (!atIntersection &&
        std::find(moves.begin(), moves.end(), currentDir) != moves.end()) {
        std::uniform_int_distribution<int> coin(0, 99);
        if (coin(rng) < 80) {
            info->out_move      = currentDir;
            info->committed_move = currentDir;
            info->commitment_ticks = 4;
            if (!nogui) std::cout << "→ Explorando (continuando corredor)\n";
            return BH_SUCCESS;
        }
    }

    // En intersección o cambio aleatorio: elegir dirección evitando retroceso
    // y priorizando rutas que no llevan a callejones
    struct OptMove { Move m; int exits; };
    std::vector<OptMove> candidates;

    // Determinar dirección opuesta para evitar retroceso
    Move opposite = PASS;
    if (currentDir == UP)    opposite = DOWN;
    else if (currentDir == DOWN)  opposite = UP;
    else if (currentDir == LEFT)  opposite = RIGHT;
    else if (currentDir == RIGHT) opposite = LEFT;

    for (Move m : moves) {
        if (m == opposite && moves.size() > 1) continue; // evitar retroceso si hay otras opciones
        int nb = gs->getMaze().getNeighbour(currentNode, m);
        if (nb < 0) continue;
        candidates.push_back({m, countMoves(nb, gs)});
    }
    if (candidates.empty()) {
        // Último recurso: cualquier movimiento
        std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
        info->out_move = moves[d(rng)];
        return BH_SUCCESS;
    }

    // Ordenar preferiendo nodos con más salidas (intersecciones primero)
    std::sort(candidates.begin(), candidates.end(),
              [](const OptMove& a, const OptMove& b) {
                  return a.exits > b.exits;
              });

    // Con algo de aleatoriedad entre los top candidatos
    int topN = std::min((int)candidates.size(), 2);
    std::uniform_int_distribution<int> pick(0, topN - 1);
    Move chosen = candidates[pick(rng)].m;

    info->out_move       = chosen;
    info->committed_move = chosen;
    info->commitment_ticks = atIntersection ? 0 : 4;

    if (!nogui) std::cout << "→ Explorando (nueva dirección)\n";
    return BH_SUCCESS;
}