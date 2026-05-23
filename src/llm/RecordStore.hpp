#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm::internals {

bool loadGameStateSeedFromJson(const std::string& path, GameState& state, std::string& error);
bool loadGameRecordsFromJson(const std::string& path, GameRecords& records, std::string& error);
bool saveGameRecordsToJson(const std::string& path, const GameRecords& records, std::string& error);
void mergeGameRecords(GameRecords& target, const GameRecords& source);

} // namespace textrpg::llm::internals
