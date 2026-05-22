#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

class ResponseParser {
public:
    static Prologue parsePrologue(const std::string& rawText);
    static GameEvent parseEvent(const std::string& rawText, const std::string& fallbackType);
    static InitialWorld parseInitialWorld(const std::string& rawText);
    static ActionResult parseActionResult(const std::string& rawText);
    static ElderDialogueResult parseElderDialogue(const std::string& rawText);
    static Prologue fallbackPrologue(const std::string& reason);
    static GameEvent fallbackEvent(const std::string& reason, const std::string& eventType);
    static InitialWorld fallbackInitialWorld(const std::string& reason);
    static ActionResult fallbackActionResult(const std::string& reason);
    static ElderDialogueResult fallbackElderDialogue(const std::string& reason);
};

} // namespace textrpg::llm::internals
