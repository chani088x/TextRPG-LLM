#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

std::string buildRagContext(const GameState& state, const std::string& query);

} // namespace textrpg::llm::internals
