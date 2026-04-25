#pragma once

#include "llm/LLMTypes.hpp"

#include <string>

namespace textrpg::llm {

// 엔진 상태를 LLM에게 넘길 짧은 요약 문자열로 만든다.
// 전체 로그나 내부 클래스 구조를 넘기지 않는 것이 이 클래스의 핵심 역할이다.
class ContextBuilder {
public:
    explicit ContextBuilder(PromptSettings settings = {});

    // playerInput은 최근 행동으로 context에도 포함되어 모델이 즉시 반응할 수 있게 한다.
    std::string build(const GameState& state, const std::string& playerInput) const;

private:
    PromptSettings settings_;
};

} // namespace textrpg::llm
