#pragma once

#include "llm/ContextBuilder.hpp"
#include "llm/ILLMClient.hpp"
#include "llm/LLMEventValidator.hpp"
#include "llm/LLMFallbackFactory.hpp"
#include "llm/LLMLogger.hpp"
#include "llm/LLMOutputParser.hpp"
#include "llm/PromptBuilder.hpp"

#include <memory>
#include <string>

namespace textrpg::llm {

// GameEngine이 호출하는 LLM 모듈의 단일 진입점이다.
// prompt 생성, LLM 호출, parse, validate, fallback 흐름을 이곳에서 묶는다.
class LLMService {
public:
    explicit LLMService(std::shared_ptr<ILLMClient> client);

    LLMService(
        std::shared_ptr<ILLMClient> client,
        ContextBuilder contextBuilder,
        PromptBuilder promptBuilder,
        LLMOutputParser parser,
        LLMEventValidator validator,
        LLMFallbackFactory fallbackFactory,
        LLMLogger logger = LLMLogger{});

    GameEvent generateEvent(const GameState& state, const std::string& playerInput);

private:
    ContextBuilder contextBuilder_;
    PromptBuilder promptBuilder_;
    std::shared_ptr<ILLMClient> client_;
    LLMOutputParser parser_;
    LLMEventValidator validator_;
    LLMFallbackFactory fallbackFactory_;
    LLMLogger logger_;
};

} // namespace textrpg::llm
