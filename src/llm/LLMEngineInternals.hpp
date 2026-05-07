#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

GameEvent parseEvent(const std::string& rawText, const std::string& fallbackType = ids::event::Story);
ActionResult parseActionResult(const std::string& rawText, const std::string& diceOutcome);
InitialWorld parseInitialWorld(const std::string& rawText);

GameEvent fallbackEvent(const std::string& reason, const std::string& eventType);
ActionResult fallbackActionResult(const std::string& reason, const std::string& diceOutcome);
InitialWorld fallbackInitialWorld(const std::string& reason);

void applyInitialWorld(GameState& state, const InitialWorld& world);
void applyEvent(GameState& state, const GameEvent& event, const std::string& actionContext);
void applyActionResult(
    GameState& state,
    const ActionResult& result,
    const std::string& diceOutcome,
    const std::string& customInput);

} // namespace textrpg::llm::internals
