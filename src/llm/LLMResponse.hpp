#pragma once

#include <string>

namespace textrpg::llm {

struct LLMResponse {
    std::string rawText;
    bool transportOk = true;
    std::string errorMessage;
};

} // namespace textrpg::llm
