#pragma once

#include <string>

namespace textrpg::llm {

// GameEngine과 서비스 계층이 특정 LLM 백엔드에 묶이지 않게 하는 경계다.
// Ollama, llama.cpp server, 테스트용 fake client는 모두 이 인터페이스만 맞추면 된다.
class ILLMClient {
public:
    virtual ~ILLMClient() = default;

    // prompt를 보내고 LLM이 생성한 원문 텍스트를 돌려준다.
    // JSON 파싱과 게임 규칙 검증은 이 인터페이스 밖의 책임이다.
    virtual std::string generate(const std::string& prompt) = 0;
};

} // namespace textrpg::llm
