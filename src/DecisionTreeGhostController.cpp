#include "DecisionTreeGhostController.h"
#include <cmath>
#include <vector>
#include <algorithm>

DecisionTreeGhostController::DecisionTreeGhostController(std::shared_ptr<Character> character)
    : Controller(character) {}

Move DecisionTreeGhostController::getMove(const GameState& gs) {
    // Determinar si el fantasma es comestible
    auto ghost = dynamic_cast<Ghost*>(character.get());
    bool edible = (ghost != nullptr && ghost->isEdible());

    // Posición actual de Ms. PacMan
    int pacmanPos = gs.getPacmanPos();
    auto targetPos = gs.getMaze().getNodePos(pacmanPos);

    // Obtener movimientos legales (sin revertir dirección a menos que sea forzoso)
    std::vector<Move> moves;
    if (character->getDirection() == PASS) {
        moves = gs.getMaze().getPossibleMoves(character->getPos());
    } else {
        moves = gs.getMaze().getGhostLegalMoves(character->getPos(), character->getDirection());
    }

    if (moves.empty()) return PASS;

    Move bestMove = moves[0];
    float bestValue = 0.0f;
    bool first = true;

    if (edible) {
        // Huir: maximizar la distancia a Ms. PacMan
        for (Move move : moves) {
            if (move == PASS) continue;
            int neighbor = gs.getMaze().getNeighbour(character->getPos(), move);
            auto neighborPos = gs.getMaze().getNodePos(neighbor);
            float dx = neighborPos.first - targetPos.first;
            float dy = neighborPos.second - targetPos.second;
            float distSq = dx*dx + dy*dy;
            if (first || distSq > bestValue) {
                bestValue = distSq;
                bestMove = move;
                first = false;
            }
        }
    } else {
        // Perseguir: minimizar la distancia a Ms. PacMan
        for (Move move : moves) {
            if (move == PASS) continue;
            int neighbor = gs.getMaze().getNeighbour(character->getPos(), move);
            auto neighborPos = gs.getMaze().getNodePos(neighbor);
            float dx = neighborPos.first - targetPos.first;
            float dy = neighborPos.second - targetPos.second;
            float distSq = dx*dx + dy*dy;
            if (first || distSq < bestValue) {
                bestValue = distSq;
                bestMove = move;
                first = false;
            }
        }
    }
    return bestMove;
}