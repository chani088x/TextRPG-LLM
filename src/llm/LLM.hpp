#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm {

struct LLMOptions {
    std::string endpoint = "http://localhost:11434";
    std::string model = "llama3.2:latest";
    double temperature = 0.7;
    bool think = false;
    int connectionTimeoutSeconds = 10;
    int readTimeoutSeconds = 300;
};

class LLM {
public:
    explicit LLM(LLMOptions options = {});

    GameEvent generateEvent(const GameState& state, const std::string& playerInput) const;

    static GameEvent parseEvent(const std::string& rawText);

private:
    LLMOptions options_;
};

} // namespace textrpg::llm
