#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

class ResponseParser {
public:
    static GameEvent parseEvent(const std::string& rawText, const std::string& fallbackType);
    static ActionResult parseActionResult(const std::string& rawText, const std::string& diceOutcome);
    static InitialWorld parseInitialWorld(const std::string& rawText);
};

} // namespace textrpg::llm::internals
