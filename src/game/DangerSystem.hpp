#pragma once

#include "llm/LLMTypes.hpp"

namespace textrpg::game {

struct DangerAdvance {
    int previous = 0;
    int increase = 0;
    int current = 0;
    int threshold = 10;
    bool combatTriggered = false;
};

class DangerSystem {
public:
    DangerAdvance advanceTurn(llm::GameState& state) const;
    void resetAfterCombat(llm::GameState& state) const;
};

} // namespace textrpg::game
