#include "llm/LLMService.hpp"

#include <exception>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace textrpg::llm {
namespace {

std::string joinMessages(const std::vector<std::string>& messages)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < messages.size(); ++i) {
        if (i != 0) {
            out << "; ";
        }
        out << messages[i];
    }
    return out.str();
}

} // namespace

LLMService::LLMService(std::shared_ptr<ILLMClient> client)
    : client_(std::move(client))
{
    if (!client_) {
        throw std::invalid_argument("LLMService requires a non-null client");
    }
}

LLMService::LLMService(
    std::shared_ptr<ILLMClient> client,
    ContextBuilder contextBuilder,
    PromptBuilder promptBuilder,
    LLMOutputParser parser,
    LLMEventValidator validator,
    LLMFallbackFactory fallbackFactory,
    LLMLogger logger)
    : contextBuilder_(std::move(contextBuilder))
    , promptBuilder_(std::move(promptBuilder))
    , client_(std::move(client))
    , parser_(std::move(parser))
    , validator_(std::move(validator))
    , fallbackFactory_(std::move(fallbackFactory))
    , logger_(std::move(logger))
{
    if (!client_) {
        throw std::invalid_argument("LLMService requires a non-null client");
    }
}

GameEvent LLMService::generateEvent(const GameState& state, const std::string& playerInput)
{
    // 이 함수 하나 안에서 LLM 모듈의 전체 파이프라인을 끝낸다.
    // 호출자는 이미 검증되었거나 fallback 처리된 GameEvent만 받는다.
    const auto context = contextBuilder_.build(state, playerInput);
    const auto prompt = promptBuilder_.build(context, playerInput);
    logger_.logPrompt(state.turnNumber, playerInput, prompt);

    std::string rawResponse;
    try {
        rawResponse = client_->generate(prompt);
    } catch (const std::exception& ex) {
        // 통신 실패도 게임 중단 대신 안전 이벤트로 바꾼다.
        logger_.logParseError(state.turnNumber, ex.what());
        logger_.logFallback(state.turnNumber, ex.what());
        return fallbackFactory_.createSafeEvent(ex.what());
    }

    logger_.logRawResponse(state.turnNumber, rawResponse);

    const auto parseResult = parser_.parse(rawResponse);
    if (!parseResult.success) {
        // JSON 계약 위반은 서비스 밖으로 새지 않게 fallback으로 수렴시킨다.
        logger_.logParseError(state.turnNumber, parseResult.errorMessage);
        logger_.logFallback(state.turnNumber, parseResult.errorMessage);
        return fallbackFactory_.createSafeEvent(parseResult.errorMessage);
    }

    const auto validationResult = validator_.validate(parseResult.event);
    logger_.logValidation(state.turnNumber, validationResult.messages);

    if (!validationResult.valid) {
        // 구조적 규칙 위반은 repair하지 않고 안전한 기본 이벤트로 대체한다.
        const auto reason = joinMessages(validationResult.messages);
        logger_.logFallback(state.turnNumber, reason);
        return fallbackFactory_.createSafeEvent(reason);
    }

    return validationResult.event;
}

} // namespace textrpg::llm
