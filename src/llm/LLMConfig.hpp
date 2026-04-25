#pragma once

#include "llm/LLMLogger.hpp"
#include "llm/LLMTypes.hpp"
#include "llm/OllamaClient.hpp"

#include <filesystem>
#include <string>

namespace textrpg::llm {

struct LLMConfig {
    std::string backend = "ollama";
    OllamaConfig ollama;
    PromptSettings prompt;
    ValidationConfig validation;
    LogSettings debug;
};

class LLMConfigLoader {
public:
    static LLMConfig load(const std::filesystem::path& path);
};

} // namespace textrpg::llm
