#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

class StateApplier {
public:
    static void applyInitialWorld(GameState& state, const InitialWorld& world);
    static void applyEvent(GameState& state, const GameEvent& event, const std::string& actionContext);
    static void applyActionResult(
        GameState& state,
        const ActionResult& result,
        const std::string& diceOutcome,
        const std::string& customInput);
};

} // namespace textrpg::llm::internals
