#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm {

struct LLMRequest {
    GameState state;
    std::string playerInput;
};

} // namespace textrpg::llm
