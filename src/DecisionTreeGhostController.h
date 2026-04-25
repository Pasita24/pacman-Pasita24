#ifndef DECISIONTREEGHOSTCONTROLLER_H_
#define DECISIONTREEGHOSTCONTROLLER_H_

#include "Controller.h"
#include "Ghost.h"
#include "GameState.h"

class DecisionTreeGhostController : public Controller {
public:
    explicit DecisionTreeGhostController(std::shared_ptr<Character> character);
    virtual Move getMove(const GameState& gs) override;
};

#endif // DECISIONTREEGHOSTCONTROLLER_H_