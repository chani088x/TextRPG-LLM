#include "llm/StateApplier.hpp"

#include <algorithm>
#include <vector>

namespace textrpg::llm::internals {
namespace {

void pushLimited(std::vector<std::string>& values, const std::string& value, std::size_t maxCount)
{
    if (value.empty()) {
        return;
    }

    values.push_back(value);
    while (values.size() > maxCount) {
        values.erase(values.begin());
    }
}

struct EventApplyPolicy {
    const char* eventType = ids::event::Story;
    bool applyStats = true;
    bool applyItem = true;
};

class EventPolicyRegistry {
public:
    static EventApplyPolicy applyPolicyFor(const std::string& eventType)
    {
        for (const auto& policy : applyPolicies()) {
            if (eventType == policy.eventType) {
                return policy;
            }
        }
        return EventApplyPolicy {};
    }

private:
    static const std::vector<EventApplyPolicy>& applyPolicies()
    {
        static const std::vector<EventApplyPolicy> policies = {
            {ids::event::Story, true, true},
            {ids::event::Combat, false, false},
            {ids::event::ItemGain, true, true},
            {ids::event::StatChange, true, true},
            {ids::event::Dialogue, true, true},
            {ids::event::QuestUpdate, true, true},
            {ids::event::Rest, true, true},
            {ids::event::GameEnd, true, true},
        };
        return policies;
    }
};

} // namespace

void StateApplier::applyInitialWorld(GameState& state, const InitialWorld& world)
{
    state.world.location = world.location;
    state.currentScene = world.sceneText;
    state.world.currentObjective = world.currentObjective;
    state.world.decisionHint = world.decisionHint;
    pushLimited(state.memory.recentEvents, "시작 상황: " + world.memoryNote, 5);
}

void StateApplier::applyEvent(GameState& state, const GameEvent& event, const std::string& actionContext)
{
    state.currentScene = event.sceneText;

    if (!event.location.empty()) {
        state.world.location = event.location;
    }
    if (!event.nextObjective.empty()) {
        state.world.currentObjective = event.nextObjective;
    }
    if (!event.decisionHint.empty()) {
        state.world.decisionHint = event.decisionHint;
    }

    const auto policy = EventPolicyRegistry::applyPolicyFor(event.eventType);
    if (policy.applyStats) {
        state.player.hp = clampInt(state.player.hp + event.statChanges.hp, 1, state.player.maxHp);
        state.player.gold = std::max(0, state.player.gold + event.statChanges.gold);
        state.player.exp = std::max(0, state.player.exp + event.statChanges.exp);
    }

    if (policy.applyItem && event.item.has_value()) {
        state.player.inventory.push_back(event.item->name);
    }

    pushLimited(state.memory.importantChoices, "턴 " + std::to_string(state.turnNumber) + " 선택: " + actionContext, 5);
    pushLimited(state.memory.recentEvents, "턴 " + std::to_string(state.turnNumber) + " 결과: " + event.memoryNote, 5);
    ++state.turnNumber;
}

void StateApplier::applyActionResult(
    GameState& state,
    const ActionResult& result,
    const std::string& diceOutcome,
    const std::string& customInput)
{
    const auto normalizedOutcome = normalizeDiceOutcome(diceOutcome);
    const int hpDelta = clampInt(result.hpDelta, 1 - state.player.hp, state.player.maxHp - state.player.hp);
    state.player.hp = clampInt(state.player.hp + hpDelta, 1, state.player.maxHp);

    const int goldDelta = isFailureOutcome(normalizedOutcome) ? 0 : result.goldDelta;
    const int expDelta = isFailureOutcome(normalizedOutcome) ? 0 : result.expDelta;
    state.player.gold = std::max(0, state.player.gold + goldDelta);
    state.player.exp = std::max(0, state.player.exp + expDelta);

    if (!isFailureOutcome(normalizedOutcome) && !result.itemName.empty()) {
        state.player.inventory.push_back(result.itemName);
    }

    pushLimited(state.memory.importantChoices, "턴 " + std::to_string(state.turnNumber) + " 고유 행동: " + customInput, 5);
    pushLimited(state.memory.recentEvents, "고유 행동 결과: " + result.resultText, 5);
}

void applyInitialWorld(GameState& state, const InitialWorld& world)
{
    StateApplier::applyInitialWorld(state, world);
}

void applyEvent(GameState& state, const GameEvent& event, const std::string& actionContext)
{
    StateApplier::applyEvent(state, event, actionContext);
}

void applyActionResult(
    GameState& state,
    const ActionResult& result,
    const std::string& diceOutcome,
    const std::string& customInput)
{
    StateApplier::applyActionResult(state, result, diceOutcome, customInput);
}

} // namespace textrpg::llm::internals
