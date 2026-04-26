#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm {

struct LLMOptions {
    std::string endpoint = "http://localhost:11434";
    std::string model = "0xIbra/supergemma4-26b-uncensored-gguf-v2:Q4_K_M";
    double temperature = 0.7;
    bool think = false;
    int connectionTimeoutSeconds = 10;
    int readTimeoutSeconds = 300;
};

class LLM {
public:
    explicit LLM(LLMOptions options = {});

    GameEvent generateEvent(const GameState& state, const std::string& playerInput) const;
    GameEvent generateNextEvent(const GameState& state, const std::string& actionContext) const;
    GameEvent generateCombatEvent(const GameState& state, const std::string& actionContext) const;
    GameEvent generateStoryEvent(const GameState& state, const std::string& actionContext) const;
    ActionResult generateActionResult(
        const GameState& state,
        const std::string& customInput,
        DiceOutcome diceOutcome) const;
    InitialWorld generateInitialWorld() const;

    static GameEvent parseEvent(const std::string& rawText);
    static GameEvent parseEvent(const std::string& rawText, EventType fallbackType);
    static ActionResult parseActionResult(const std::string& rawText, DiceOutcome diceOutcome);
    static InitialWorld parseInitialWorld(const std::string& rawText);

private:
    LLMOptions options_;
};

} // namespace textrpg::llm
