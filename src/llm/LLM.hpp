#pragma once

#include "llm/LLMTypes.hpp"

#include <memory>
#include <string>

namespace textrpg::llm {

enum class LLMProvider {
    OpenAI,
    Ollama
};

struct LLMOptions {
    LLMProvider provider = LLMProvider::OpenAI;
    std::string endpoint;
    std::string model;
    std::string apiKey;
    std::string organization;
    double temperature = 0.7;
    bool think = false;
    int connectionTimeoutSeconds = 10;
    int readTimeoutSeconds = 300;
};

class IChatClient {
public:
    virtual ~IChatClient() = default;
    virtual std::string chat(const std::string& prompt) const = 0;
};

class LLM {
public:
    explicit LLM(LLMOptions options = {});
    explicit LLM(std::shared_ptr<IChatClient> chatClient);

    GameEvent generateEvent(GameState& state, const std::string& playerInput) const;
    GameEvent generateNextEvent(GameState& state, const std::string& actionContext) const;
    GameEvent generateCombatEvent(GameState& state, const std::string& actionContext) const;
    GameEvent generateStoryEvent(GameState& state, const std::string& actionContext) const;
    ActionResult generateActionResult(
        GameState& state,
        const std::string& customInput,
        const std::string& diceOutcome) const;
    InitialWorld generateInitialWorld(GameState& state) const;

private:
    std::shared_ptr<IChatClient> chatClient_;
};

} // namespace textrpg::llm
