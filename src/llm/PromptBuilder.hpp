#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

class PromptBuilder {
public:
    static std::string buildProloguePrompt(const GameState& state);
    static std::string buildNextEventPrompt(const GameState& state, const std::string& actionContext);
    static std::string buildActionResultPrompt(
        const GameState& state,
        const std::string& customInput,
        const std::string& diceOutcome);
    static std::string buildElderDialoguePrompt(const GameState& state);
    static std::string buildInitialWorldPrompt(const GameState& state);
};

} // namespace textrpg::llm::internals
