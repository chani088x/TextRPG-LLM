#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

class PromptBuilder {
public:
    static std::string buildCombatPrompt(const GameState& state, const std::string& actionContext);
    static std::string buildStoryPrompt(const GameState& state, const std::string& actionContext);
    static std::string buildNextEventPrompt(const GameState& state, const std::string& actionContext);
    static std::string buildActionResultPrompt(
        const GameState& state,
        const std::string& customInput,
        const std::string& diceOutcome);
    static std::string buildInitialWorldPrompt();
};

} // namespace textrpg::llm::internals
