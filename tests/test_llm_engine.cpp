#include "llm/LLM.hpp"
#include "llm/LLMEngineInternals.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

using namespace textrpg::llm;

namespace lli = textrpg::llm::internals;

namespace {

bool hasNote(const std::vector<std::string>& notes, const std::string& text)
{
    for (const auto& note : notes) {
        if (note.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

GameState makeState()
{
    GameState state;
    state.turnNumber = 3;
    state.currentScene = "old scene";
    state.player.hp = 10;
    state.player.maxHp = 20;
    state.player.gold = 1;
    state.player.exp = 2;
    state.player.inventory = {"torch"};
    state.world.location = "old location";
    state.world.currentObjective = "old objective";
    state.world.decisionHint = "old hint";
    return state;
}

void parseValidEventJson()
{
    const auto event = lli::parseEvent(R"({
        "scene_text": "found a key",
        "location": "ruins",
        "event_type": "item_gain",
        "next_objective": "open gate",
        "decision_hint": "inspect or advance",
        "choices": ["inspect", "advance"],
        "monster": null,
        "item": {"name": "bronze key", "type": "quest_item", "description": "old", "value": 5},
        "stat_changes": {"hp": -3, "gold": 10, "exp": 4},
        "memory_note": "key found"
    })", "");

    assert(event.eventType == ids::event::ItemGain);
    assert(event.sceneText == "found a key");
    assert(event.location == "ruins");
    assert(event.item.has_value());
    assert(event.item->type == ids::item::QuestItem);
    assert(event.statChanges.hp == -3);
}

void repairUnknownEventType()
{
    const auto event = lli::parseEvent(R"({
        "scene_text": "strange",
        "location": "road",
        "event_type": "mystery",
        "next_objective": "continue",
        "decision_hint": "choose",
        "choices": ["a", "b"],
        "monster": null,
        "item": null,
        "stat_changes": {"hp": 0, "gold": 0, "exp": 0},
        "memory_note": "unknown type"
    })", "");

    assert(event.eventType == ids::event::Story);
    assert(hasNote(event.validationNotes, "event_type"));
}

void applyNonCombatEventToState()
{
    auto state = makeState();
    auto event = lli::parseEvent(R"({
        "scene_text": "new scene",
        "location": "new place",
        "event_type": "item_gain",
        "next_objective": "new objective",
        "decision_hint": "new hint",
        "choices": ["a", "b"],
        "monster": null,
        "item": {"name": "map", "type": "quest_item", "description": "", "value": 1},
        "stat_changes": {"hp": -4, "gold": 7, "exp": 8},
        "memory_note": "event memory"
    })", "");

    lli::applyEvent(state, event, "action context");

    assert(state.currentScene == "new scene");
    assert(state.world.location == "new place");
    assert(state.world.currentObjective == "new objective");
    assert(state.world.decisionHint == "new hint");
    assert(state.player.hp == 6);
    assert(state.player.gold == 8);
    assert(state.player.exp == 10);
    assert(state.player.inventory.back() == "map");
    assert(state.turnNumber == 4);
}

void combatEventDoesNotApplyLlmStats()
{
    auto state = makeState();
    auto event = lli::parseEvent(R"({
        "scene_text": "enemy appears",
        "location": "arena",
        "event_type": "combat",
        "next_objective": "defeat enemy",
        "decision_hint": "fight",
        "choices": [],
        "monster": {"name": "bandit", "description": "armed"},
        "item": {"name": "coin", "type": "consumable", "description": "", "value": 1},
        "stat_changes": {"hp": -30, "gold": 100, "exp": 50},
        "memory_note": "combat starts"
    })", "");

    lli::applyEvent(state, event, "advance");

    assert(state.player.hp == 10);
    assert(state.player.gold == 1);
    assert(state.player.exp == 2);
    assert(state.player.inventory.size() == 1);
    assert(state.turnNumber == 4);
}

void actionResultFailureBlocksRewards()
{
    auto state = makeState();
    const auto result = lli::parseActionResult(R"({
        "result_text": "failed attempt",
        "result_type": "item",
        "item_name": "gem",
        "item_description": "shiny",
        "hp_delta": 5,
        "gold_delta": 99,
        "exp_delta": 99
    })", ids::dice::Failure);

    lli::applyActionResult(state, result, ids::dice::Failure, "try");

    assert(state.player.hp == 10);
    assert(state.player.gold == 1);
    assert(state.player.exp == 2);
    assert(state.player.inventory.size() == 1);
    assert(hasNote(result.notes, "failure rewards"));
}

void actionResultHpClamp()
{
    auto state = makeState();
    state.player.hp = 3;
    const auto result = lli::parseActionResult(R"({
        "result_text": "hurt",
        "result_type": "damage",
        "item_name": "",
        "item_description": "",
        "hp_delta": -20,
        "gold_delta": 0,
        "exp_delta": 0
    })", ids::dice::Success);

    lli::applyActionResult(state, result, ids::dice::Success, "risk");

    assert(state.player.hp == 1);
}

void applyInitialWorldToState()
{
    auto state = makeState();
    const auto world = lli::parseInitialWorld(R"({
        "location": "harbor",
        "scene_text": "ships wait",
        "current_objective": "find captain",
        "decision_hint": "ask around",
        "memory_note": "started at harbor"
    })");

    lli::applyInitialWorld(state, world);

    assert(state.world.location == "harbor");
    assert(state.currentScene == "ships wait");
    assert(state.world.currentObjective == "find captain");
    assert(state.world.decisionHint == "ask around");
    assert(!state.memory.recentEvents.empty());
}

class FakeChatClient final : public IChatClient {
public:
    explicit FakeChatClient(std::string response)
        : response_(std::move(response))
    {
    }

    std::string chat(const std::string&) const override
    {
        return response_;
    }

private:
    std::string response_;
};

void generatedEventUsesInjectedClientAndAppliesState()
{
    auto state = makeState();
    auto client = std::make_shared<FakeChatClient>(R"({
        "scene_text": "generated scene",
        "location": "tower",
        "event_type": "quest_update",
        "next_objective": "climb",
        "decision_hint": "move carefully",
        "choices": ["up", "down"],
        "monster": null,
        "item": null,
        "stat_changes": {"hp": 2, "gold": 3, "exp": 4},
        "memory_note": "generated"
    })");

    LLM llm(client);
    const auto event = llm.generateNextEvent(state, "advance");

    assert(event.eventType == ids::event::QuestUpdate);
    assert(state.currentScene == "generated scene");
    assert(state.world.location == "tower");
    assert(state.player.hp == 12);
    assert(state.player.gold == 4);
    assert(state.player.exp == 6);
}

} // namespace

int main()
{
    parseValidEventJson();
    repairUnknownEventType();
    applyNonCombatEventToState();
    combatEventDoesNotApplyLlmStats();
    actionResultFailureBlocksRewards();
    actionResultHpClamp();
    applyInitialWorldToState();
    generatedEventUsesInjectedClientAndAppliesState();
    return 0;
}
