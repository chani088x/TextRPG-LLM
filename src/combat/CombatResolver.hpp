#pragma once

#include "combat/CombatTypes.hpp"
#include "llm/LLMTypes.hpp"

#include <optional>
#include <string>

namespace textrpg::combat {

struct CombatResolution {
    std::string logText;
    std::string actionContext;
};

class CombatResolver {
public:
    bool isActive() const;
    void updateFromEvent(const llm::GameEvent& event);
    CombatResolution resolvePlayerAttack(llm::GameState& state);
    CombatResolution resolveCustomAction(
        llm::GameState& state,
        const std::string& actionText,
        const std::string& diceOutcome,
        int diceValue);
    CombatResolution resolveItemUse(llm::GameState& state);

private:
    std::optional<llm::Monster> activeMonster_;
};

} // namespace textrpg::combat
