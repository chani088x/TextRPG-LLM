#pragma once

#include <string>

namespace textrpg::llm {

// context와 플레이어 입력을 합쳐 LLM용 최종 프롬프트를 만든다.
// 출력 JSON 스키마와 금지 규칙을 한곳에 고정해 응답 형식을 안정화한다.
class PromptBuilder {
public:
    std::string build(const std::string& context, const std::string& playerInput) const;
};

} // namespace textrpg::llm
