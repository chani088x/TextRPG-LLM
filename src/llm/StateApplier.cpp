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

void pushUniqueLimited(std::vector<std::string>& values, const std::string& value, std::size_t maxCount)
{
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        pushLimited(values, value, maxCount);
    }
}

void rememberItem(GameState& state, const Item& item)
{
    if (item.name.empty()) {
        return;
    }

    const auto sameName = [&item](const Item& existing) {
        return existing.name == item.name;
    };
    if (std::find_if(state.records.obtainedItems.begin(), state.records.obtainedItems.end(), sameName)
        == state.records.obtainedItems.end()) {
        state.records.obtainedItems.push_back(item);
    }
}

void rememberEnemy(GameState& state, const Monster& monster)
{
    if (monster.name.empty()) {
        return;
    }

    const auto sameName = [&monster](const Monster& existing) {
        return existing.name == monster.name;
    };
    if (std::find_if(state.records.encounteredEnemies.begin(), state.records.encounteredEnemies.end(), sameName)
        == state.records.encounteredEnemies.end()) {
        state.records.encounteredEnemies.push_back(monster);
    }
}

void rememberEvent(GameState& state, const GameEvent& event)
{
    GameRecords::EventRecord record;
    record.turnNumber = state.turnNumber;
    record.eventType = normalizeEventType(event.eventType);
    record.eventLabel = eventTypeToString(record.eventType);
    record.summary = event.memoryNote.empty() ? event.sceneText : event.memoryNote;

    state.records.eventHistory.push_back(std::move(record));
    while (state.records.eventHistory.size() > 100) {
        state.records.eventHistory.erase(state.records.eventHistory.begin());
    }
}

bool hasInventoryItem(const GameState& state, const std::string& name)
{
    return std::find_if(state.player.inventory.begin(), state.player.inventory.end(), [&name](const Item& item) {
        return item.name == name;
    }) != state.player.inventory.end();
}

bool hasMainObjective(const GameRecords& records)
{
    return records.elder.talked || records.boss.known;
}

void applyItemToPlayer(GameState& state, const Item& item)
{
    if (item.name.empty()) {
        return;
    }

    const bool alreadyOwned = hasInventoryItem(state, item.name);
    if (item.type == ids::item::Consumable) {
        state.player.inventory.push_back(item);
        return;
    }

    if (!alreadyOwned) {
        state.player.inventory.push_back(item);
    }

    if (alreadyOwned) {
        return;
    }

    if (item.type == ids::item::Weapon) {
        state.player.attack += std::max(1, item.value / 10);
    } else if (item.type == ids::item::Armor) {
        state.player.defense += std::max(1, item.value / 10);
    }
}

void applyStatsToPlayer(GameState& state, const StatChanges& changes)
{
    state.player.hp = clampInt(state.player.hp + changes.hp, 1, state.player.maxHp);
    state.player.gold = std::max(0, state.player.gold + changes.gold);
    state.player.exp = std::max(0, state.player.exp + changes.exp);
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

void StateApplier::applyPrologue(GameState& state, const Prologue& prologue)
{
    if (!prologue.generated) {
        return;
    }

    state.records.prologue = prologue;
    state.records.prologue.generated = true;
    state.currentScene = prologue.text;
    if (!prologue.openingLocation.empty()) {
        state.world.location = prologue.openingLocation;
    }
    if (!prologue.firstObjective.empty()) {
        state.world.currentObjective = prologue.firstObjective;
    }
    state.world.baseCandidate = false;
}

void StateApplier::applyInitialWorld(GameState& state, const InitialWorld& world)
{
    state.world.location = world.location;
    state.currentScene = world.sceneText;
    state.world.currentObjective = world.currentObjective;
    state.world.decisionHint = world.decisionHint;
    state.world.baseCandidate = !state.records.base.unlocked && world.baseCandidate;
    if (hasMainObjective(state.records)) {
        pushUniqueLimited(state.records.questLog, world.currentObjective, 20);
    }
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
        if (hasMainObjective(state.records)) {
            pushUniqueLimited(state.records.questLog, event.nextObjective, 20);
        }
    }
    if (!event.decisionHint.empty()) {
        state.world.decisionHint = event.decisionHint;
    }
    state.world.baseCandidate = !state.records.base.unlocked && event.baseCandidate;

    const auto policy = EventPolicyRegistry::applyPolicyFor(event.eventType);
    if (event.monster.has_value()) {
        rememberEnemy(state, event.monster.value());
    }

    if (policy.applyStats) {
        applyStatsToPlayer(state, event.statChanges);
    }

    if (policy.applyItem && event.item.has_value()) {
        applyItemToPlayer(state, event.item.value());
        rememberItem(state, event.item.value());
    }

    pushLimited(state.memory.importantChoices, "턴 " + std::to_string(state.turnNumber) + " 선택: " + actionContext, 5);
    pushLimited(state.memory.recentEvents, event.memoryNote, 5);
    rememberEvent(state, event);
    ++state.turnNumber;
}

void StateApplier::applyActionResult(GameState& state, const ActionResult& result, const std::string& customInput)
{
    state.currentScene = result.resultText;

    if (!result.location.empty()) {
        state.world.location = result.location;
    }
    state.world.baseCandidate = !state.records.base.unlocked && result.baseCandidate;

    applyStatsToPlayer(state, result.statChanges);

    if (result.item.has_value()) {
        applyItemToPlayer(state, result.item.value());
        rememberItem(state, result.item.value());
    }

    pushLimited(state.memory.importantChoices, "턴 " + std::to_string(state.turnNumber) + " 고유 행동: " + customInput, 5);
    pushLimited(state.memory.recentEvents, result.memoryNote, 5);
}

void StateApplier::applyElderDialogue(GameState& state, const ElderDialogueResult& result)
{
    if (state.records.elder.talked) {
        pushLimited(state.memory.recentEvents, "이미 보스 정보를 들었다.", 5);
        return;
    }

    state.currentScene = result.dialogue;
    state.records.elder.introduced = true;
    state.records.elder.talked = true;
    state.records.boss = result.boss;
    state.records.boss.known = true;

    if (!result.boss.location.empty()) {
        state.world.currentObjective = result.boss.location + "으로 향해 " + result.boss.name + "의 위협을 확인한다";
    } else {
        state.world.currentObjective = "장로에게 들은 보스 정보를 따라 최종 위협을 추적한다";
    }
    state.world.decisionHint = "거점에서 정비한 뒤 보스 정보가 가리키는 목적지로 향할지 판단한다";

    pushUniqueLimited(state.records.questLog, result.questUpdate, 20);
    if (!result.boss.name.empty()) {
        pushUniqueLimited(state.records.questLog, "보스 정보 획득: " + result.boss.name, 20);
    }
    pushLimited(state.memory.importantChoices, "턴 " + std::to_string(state.turnNumber) + " 장로와 대화", 5);
    pushLimited(state.memory.recentEvents, result.memoryNote, 5);
    ++state.turnNumber;
}

} // namespace textrpg::llm::internals
