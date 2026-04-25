#pragma once

#include "llm/LLMTypes.hpp"

#include <string>
#include <vector>

namespace textrpg::llm {

// Validator는 치명적 오류와 고칠 수 있는 오류를 나눠서 보고한다.
// valid=false면 서비스 계층에서 fallback 이벤트로 바꾼다.
struct ValidationResult {
    bool valid = false;
    bool repaired = false;
    GameEvent event;
    std::vector<std::string> messages;
};

// 파싱된 GameEvent 후보가 엔진 규칙 안에 있는지 확인한다.
// 일부 수치 초과는 clamp로 repair하고, 구조적 오류는 실패로 처리한다.
class LLMEventValidator {
public:
    explicit LLMEventValidator(ValidationConfig config = {});

    ValidationResult validate(const GameEvent& event) const;

private:
    ValidationConfig config_;
};

} // namespace textrpg::llm
