#pragma once

#include "llm/LLMTypes.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace textrpg::llm {

// 파싱 결과와 실패 이유를 함께 들고 다닌다.
// 실패해도 예외를 밖으로 던지지 않고 LLMService가 fallback으로 이어가게 한다.
struct ParseResult {
    bool success = false;
    GameEvent event;
    std::string errorMessage;
    nlohmann::json extractedJson;
};

// LLM 원문 응답에서 JSON 객체를 찾고 GameEvent 후보로 변환한다.
// 밸런스나 게임 규칙 판단은 LLMEventValidator가 담당한다.
class LLMOutputParser {
public:
    ParseResult parse(const std::string& rawText) const;

private:
    static bool extractJsonObject(const std::string& rawText, std::string& jsonText, std::string& errorMessage);
    static bool parseMonster(const nlohmann::json& value, std::optional<Monster>& monster, std::string& errorMessage);
    static bool parseItem(const nlohmann::json& value, std::optional<Item>& item, std::string& errorMessage);
};

} // namespace textrpg::llm
