#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm {

// LLM 통신, 파싱, 검증 실패가 게임 정지로 이어지지 않게 하는 안전장치다.
class LLMFallbackFactory {
public:
    GameEvent createSafeEvent(const std::string& reason = "LLM 응답 오류") const;
};

} // namespace textrpg::llm
