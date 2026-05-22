#include "llm/LLM.hpp"
#include "llm/OllamaChatClient.hpp"
#include "llm/PromptBuilder.hpp"
#include "llm/ResponseParser.hpp"
#include "llm/StateApplier.hpp"

#include <algorithm>
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
    return std::make_shared<OllamaChatClient>(std::move(options));
}

const Item* findKnownItem(const GameState& state, const std::string& name)
{
    const auto recordIt = std::find_if(
        state.records.obtainedItems.begin(),
        state.records.obtainedItems.end(),
        [&name](const Item& item) {
            return item.name == name;
        });
    if (recordIt != state.records.obtainedItems.end()) {
        return &*recordIt;
    }

    const auto inventoryIt = std::find_if(
        state.player.inventory.begin(),
        state.player.inventory.end(),
        [&name](const Item& item) {
            return item.name == name;
        });
    return inventoryIt != state.player.inventory.end() ? &*inventoryIt : nullptr;
}

void stabilizeItemAgainstState(
    const GameState& state,
    std::optional<Item>& item,
    std::vector<std::string>& validationNotes)
{
    if (!item.has_value()) {
        return;
    }

    sanitizeItem(item.value(), &validationNotes);
    if (const auto* known = findKnownItem(state, item->name)) {
        item = *known;
        sanitizeItem(item.value(), &validationNotes);
        validationNotes.push_back("known item metadata was preserved");
    }
}

bool isJackpotOutcomeText(const std::string& text)
{
    return text.find("-> 초대박") != std::string::npos
        || text.find("-> jackpot") != std::string::npos
        || text.find("outcome=jackpot") != std::string::npos;
}

void ensureJackpotActionItem(ActionResult& result, const std::string& diceOutcome)
{
    if (!isJackpotOutcomeText(diceOutcome) || result.item.has_value()) {
        return;
    }

    result.item = Item {
        "행운의 전리품",
        ids::item::Consumable,
        "초대박 판정으로 얻은 회복용 소모품.",
        10,
    };
    result.validationNotes.push_back("jackpot fallback item was granted");
}

void alignFallbackInitialWorldWithPrologue(const GameState& state, InitialWorld& world)
{
    if (!world.usedFallback || !state.records.prologue.generated) {
        return;
    }

    const auto& prologue = state.records.prologue;
    if (!prologue.openingLocation.empty()) {
        world.location = prologue.openingLocation;
    }
    if (!prologue.firstObjective.empty()) {
        world.currentObjective = prologue.firstObjective;
    }
    if (!prologue.memoryNote.empty()) {
        world.memoryNote = prologue.memoryNote;
    }
    if (!prologue.openingLocation.empty()) {
        world.sceneText = prologue.openingLocation + "에서, 당신은 프롤로그의 기억을 붙잡고 첫 실마리를 찾기 시작한다.";
    }
    world.decisionHint = "주인공의 개인 동기를 따라 단서를 확인할지, 주변의 안전을 먼저 살필지 판단해야 한다";
    world.baseCandidate = false;
    world.notes.push_back("initial fallback was aligned with prologue");
}

GameEvent generateEventWithFallbackType(
    const std::shared_ptr<IChatClient>& chatClient,
    GameState& state,
    const std::string& actionContext,
    const std::string& fallbackType,
    bool forbidCombat = false)
{
    GameEvent event;
    try {
        auto prompt = internals::PromptBuilder::buildNextEventPrompt(state, actionContext);
        if (isKnownEventType(fallbackType)) {
            prompt += "\n[엔진 요청]\n이번 응답의 event_type은 ";
            prompt += fallbackType;
            prompt += "로 맞춘다.\n";
        } else if (forbidCombat) {
            prompt += "\n[엔진 요청]\n";
            prompt += "이번 응답에서 combat event_type은 금지한다. 위험도가 아직 전투 조건에 도달하지 않았다.\n";
        }
        event = internals::ResponseParser::parseEvent(
            requireChatClient(chatClient).chat(prompt),
            fallbackType);
    } catch (const std::exception& ex) {
        event = internals::ResponseParser::fallbackEvent(ex.what(), fallbackType);
    }

    if (forbidCombat && event.eventType == ids::event::Combat) {
        event.eventType = ids::event::Story;
        event.monster.reset();
        event.validationNotes.push_back("combat event was blocked by danger system");
        if (event.choices.empty()) {
            event.choices = {
                "눈앞의 단서를 살핀다",
                "목표 방향으로 움직인다",
            };
        }
    }

    stabilizeItemAgainstState(state, event.item, event.validationNotes);
    internals::StateApplier::applyEvent(state, event, actionContext);
    return event;
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

Prologue LLM::generatePrologue(GameState& state) const
{
    if (state.records.prologue.generated) {
        internals::StateApplier::applyPrologue(state, state.records.prologue);
        return state.records.prologue;
    }

    Prologue prologue;
    try {
        const auto prompt = internals::PromptBuilder::buildProloguePrompt(state);
        prologue = internals::ResponseParser::parsePrologue(
            requireChatClient(chatClient_).chat(prompt));
    } catch (const std::exception& ex) {
        prologue = internals::ResponseParser::fallbackPrologue(ex.what());
    }

    internals::StateApplier::applyPrologue(state, prologue);
    return prologue;
}

GameEvent LLM::generateEvent(GameState& state, const std::string& playerInput) const
{
    return generateNextEvent(state, playerInput);
}

GameEvent LLM::generateNextEvent(GameState& state, const std::string& actionContext) const
{
    return generateEventWithFallbackType(chatClient_, state, actionContext, "");
}

GameEvent LLM::generateNonCombatEvent(GameState& state, const std::string& actionContext) const
{
    return generateEventWithFallbackType(chatClient_, state, actionContext, "", true);
}

GameEvent LLM::generateCombatEvent(GameState& state, const std::string& actionContext) const
{
    return generateEventWithFallbackType(chatClient_, state, actionContext, ids::event::Combat);
}

GameEvent LLM::generateStoryEvent(GameState& state, const std::string& actionContext) const
{
    return generateEventWithFallbackType(chatClient_, state, actionContext, ids::event::Story);
}

ActionResult LLM::generateActionResult(
    GameState& state,
    const std::string& customInput,
    const std::string& diceOutcome) const
{
    ActionResult result;
    try {
        const auto prompt = internals::PromptBuilder::buildActionResultPrompt(state, customInput, diceOutcome);
        result = internals::ResponseParser::parseActionResult(
            requireChatClient(chatClient_).chat(prompt));
    } catch (const std::exception& ex) {
        result = internals::ResponseParser::fallbackActionResult(ex.what());
    }

    stabilizeItemAgainstState(state, result.item, result.validationNotes);
    ensureJackpotActionItem(result, diceOutcome);
    stabilizeItemAgainstState(state, result.item, result.validationNotes);
    internals::StateApplier::applyActionResult(state, result, customInput);
    return result;
}

ElderDialogueResult LLM::generateElderDialogue(GameState& state) const
{
    if (state.records.elder.talked) {
        auto result = internals::ResponseParser::fallbackElderDialogue("elder dialogue was already completed");
        result.dialogue = "장로는 이미 필요한 이야기를 모두 전했다. 남은 것은 당신의 선택뿐이다.";
        result.boss = state.records.boss;
        result.questUpdate = "장로 대화는 이미 완료되었다.";
        result.memoryNote = "이미 완료된 장로 대화를 다시 시도했다.";
        result.usedFallback = true;
        result.validationNotes.push_back("one-time elder dialogue was blocked");
        return result;
    }

    ElderDialogueResult result;
    try {
        const auto prompt = internals::PromptBuilder::buildElderDialoguePrompt(state);
        result = internals::ResponseParser::parseElderDialogue(
            requireChatClient(chatClient_).chat(prompt));
    } catch (const std::exception& ex) {
        result = internals::ResponseParser::fallbackElderDialogue(ex.what());
    }

    internals::StateApplier::applyElderDialogue(state, result);
    return result;
}

InitialWorld LLM::generateInitialWorld(GameState& state) const
{
    InitialWorld world;
    try {
        const auto prompt = internals::PromptBuilder::buildInitialWorldPrompt(state);
        world = internals::ResponseParser::parseInitialWorld(
            requireChatClient(chatClient_).chat(prompt));
    } catch (const std::exception& ex) {
        world = internals::ResponseParser::fallbackInitialWorld(ex.what());
    }

    alignFallbackInitialWorldWithPrologue(state, world);
    internals::StateApplier::applyInitialWorld(state, world);
    return world;
}

} // namespace textrpg::llm
