#include "llm/LLM.hpp"
#include "llm/LLMEngineInternals.hpp"
#include "llm/OpenAIChatClient.hpp"
#include "llm/OllamaChatClient.hpp"
#include "llm/PromptBuilder.hpp"
#include "llm/ResponseParser.hpp"

#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

namespace textrpg::llm {
namespace {

const IChatClient& requireChatClient(const std::shared_ptr<IChatClient>& chatClient)
{
    if (!chatClient) {
        throw std::runtime_error("chat client was null");
    }
    return *chatClient;
}

std::shared_ptr<IChatClient> makeChatClient(LLMOptions options)
{
    if (options.provider == LLMProvider::Ollama) {
        return std::make_shared<OllamaChatClient>(std::move(options));
    }
    return std::make_shared<OpenAIChatClient>(std::move(options));
}

} // namespace

LLM::LLM(LLMOptions options)
    : chatClient_(makeChatClient(std::move(options)))
{
}

LLM::LLM(std::shared_ptr<IChatClient> chatClient)
    : chatClient_(std::move(chatClient))
{
}

GameEvent LLM::generateEvent(GameState& state, const std::string& playerInput) const
{
    return generateNextEvent(state, playerInput);
}

GameEvent LLM::generateNextEvent(GameState& state, const std::string& actionContext) const
{
    GameEvent event;
    try {
        const auto prompt = internals::PromptBuilder::buildNextEventPrompt(state, actionContext);
        event = internals::ResponseParser::parseEvent(
            requireChatClient(chatClient_).chat(prompt),
            "");
    } catch (const std::exception& ex) {
        event = internals::fallbackEvent(ex.what(), ids::event::Story);
    }

    internals::applyEvent(state, event, actionContext);
    return event;
}

GameEvent LLM::generateCombatEvent(GameState& state, const std::string& actionContext) const
{
    GameEvent event;
    try {
        const auto prompt = internals::PromptBuilder::buildCombatPrompt(state, actionContext);
        event = internals::ResponseParser::parseEvent(
            requireChatClient(chatClient_).chat(prompt),
            ids::event::Combat);
    } catch (const std::exception& ex) {
        event = internals::fallbackEvent(ex.what(), ids::event::Combat);
    }

    internals::applyEvent(state, event, actionContext);
    return event;
}

GameEvent LLM::generateStoryEvent(GameState& state, const std::string& actionContext) const
{
    GameEvent event;
    try {
        const auto prompt = internals::PromptBuilder::buildStoryPrompt(state, actionContext);
        event = internals::ResponseParser::parseEvent(
            requireChatClient(chatClient_).chat(prompt),
            ids::event::Story);
    } catch (const std::exception& ex) {
        event = internals::fallbackEvent(ex.what(), ids::event::Story);
    }

    internals::applyEvent(state, event, actionContext);
    return event;
}

ActionResult LLM::generateActionResult(
    GameState& state,
    const std::string& customInput,
    const std::string& diceOutcome) const
{
    const auto normalizedOutcome = normalizeDiceOutcome(diceOutcome);

    ActionResult result;
    try {
        const auto prompt = internals::PromptBuilder::buildActionResultPrompt(state, customInput, normalizedOutcome);
        result = internals::ResponseParser::parseActionResult(
            requireChatClient(chatClient_).chat(prompt),
            normalizedOutcome);
    } catch (const std::exception& ex) {
        result = internals::fallbackActionResult(ex.what(), normalizedOutcome);
    }

    internals::applyActionResult(state, result, normalizedOutcome, customInput);
    return result;
}

InitialWorld LLM::generateInitialWorld(GameState& state) const
{
    InitialWorld world;
    try {
        const auto prompt = internals::PromptBuilder::buildInitialWorldPrompt();
        world = internals::ResponseParser::parseInitialWorld(
            requireChatClient(chatClient_).chat(prompt));
    } catch (const std::exception& ex) {
        world = internals::fallbackInitialWorld(ex.what());
    }

    internals::applyInitialWorld(state, world);
    return world;
}

} // namespace textrpg::llm
