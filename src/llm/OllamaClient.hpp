#pragma once

#include "llm/ILLMClient.hpp"

#include <string>

namespace textrpg::llm {

// Ollama 연결 설정이다. 실제 값은 나중에 config/llm.toml 로더가 생기면 여기로 주입하면 된다.
struct OllamaConfig {
    std::string endpoint = "http://localhost:11434";
    // Kept for config compatibility; ollama-hpp chooses the API path internally.
    std::string apiPath = "/api/chat";
    std::string model = "gemma3";
    double temperature = 0.7;
    // This adapter currently uses the non-streaming chat API.
    bool stream = false;
    // Ollama thinking-mode switch. false matches CLI `/set nothink`.
    bool think = false;
    int connectionTimeoutSeconds = 10;
    int readTimeoutSeconds = 300;
};

// ollama.hpp를 ILLMClient 인터페이스 뒤에 숨기는 어댑터다.
// 이 클래스는 통신만 담당하고, GameEvent 변환은 parser/validator가 담당한다.
class OllamaClient final : public ILLMClient {
public:
    explicit OllamaClient(OllamaConfig config = {});

    std::string generate(const std::string& prompt) override;

private:
    OllamaConfig config_;
};

} // namespace textrpg::llm
