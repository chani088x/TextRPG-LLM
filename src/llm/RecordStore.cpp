#include "llm/RecordStore.hpp"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <utility>

namespace textrpg::llm::internals {
namespace {

using json = nlohmann::json;

std::vector<std::string> stringArrayFromJson(const json& value);

json itemToJson(const Item& item)
{
    return json {
        {"name", item.name},
        {"type", item.type},
        {"description", item.description},
        {"value", item.value},
    };
}

json enemyToJson(const Monster& monster)
{
    return json {
        {"name", monster.name},
        {"description", monster.description},
        {"hp", monster.hp},
        {"attack", monster.attack},
        {"defense", monster.defense},
    };
}

json eventRecordToJson(const GameRecords::EventRecord& record)
{
    return json {
        {"turn", record.turnNumber},
        {"event_type", record.eventType},
        {"event_label", record.eventLabel},
        {"summary", record.summary},
    };
}

json bossToJson(const BossInfo& boss)
{
    return json {
        {"known", boss.known},
        {"name", boss.name},
        {"location", boss.location},
        {"weakness", boss.weakness},
        {"description", boss.description},
    };
}

json prologueToJson(const Prologue& prologue)
{
    return json {
        {"generated", prologue.generated},
        {"text", prologue.text},
        {"protagonist_wound", prologue.protagonistWound},
        {"personal_goal", prologue.personalGoal},
        {"opening_location", prologue.openingLocation},
        {"first_clue", prologue.firstObjective},
        {"memory_note", prologue.memoryNote},
        {"used_fallback", prologue.usedFallback},
    };
}

json playerToJson(const PlayerSnapshot& player)
{
    json root {
        {"hp", player.hp},
        {"max_hp", player.maxHp},
        {"level", player.level},
        {"attack", player.attack},
        {"defense", player.defense},
        {"gold", player.gold},
        {"exp", player.exp},
        {"inventory", json::array()},
    };
    for (const auto& item : player.inventory) {
        root["inventory"].push_back(itemToJson(item));
    }
    return root;
}

json worldToJson(const WorldState& world)
{
    return json {
        {"location", world.location},
        {"current_objective", world.currentObjective},
        {"decision_hint", world.decisionHint},
        {"base_candidate", world.baseCandidate},
        {"fixed_rules", world.fixedRules},
    };
}

json memoryToJson(const StoryMemory& memory)
{
    return json {
        {"recent_events", memory.recentEvents},
        {"important_choices", memory.importantChoices},
    };
}

json snapshotToJson(const GameRecords::SnapshotState& snapshot)
{
    return json {
        {"saved", snapshot.saved},
        {"turn_number", snapshot.turnNumber},
        {"current_scene", snapshot.currentScene},
        {"player", playerToJson(snapshot.player)},
        {"world", worldToJson(snapshot.world)},
        {"memory", memoryToJson(snapshot.memory)},
    };
}

json dangerToJson(const GameRecords::DangerState& danger)
{
    return json {
        {"level", danger.level},
        {"last_increase", danger.lastIncrease},
        {"threshold", danger.threshold},
    };
}

Item itemFromJson(const json& value)
{
    Item item;
    item.name = value.value("name", std::string {});
    item.type = value.value("type", std::string(ids::item::Consumable));
    item.description = value.value("description", std::string {});
    item.value = value.value("value", 0);
    sanitizeItem(item);
    return item;
}

Monster enemyFromJson(const json& value)
{
    Monster monster;
    monster.name = value.value("name", std::string {});
    monster.description = value.value("description", std::string {});
    monster.hp = clampInt(value.value("hp", 1), 1, 60);
    monster.attack = clampInt(value.value("attack", 1), 1, 12);
    monster.defense = clampInt(value.value("defense", 0), 0, 8);
    return monster;
}

GameRecords::EventRecord eventRecordFromJson(const json& value)
{
    GameRecords::EventRecord record;
    if (!value.is_object()) {
        return record;
    }

    record.turnNumber = clampInt(value.value("turn", 0), 0, 9999);
    record.eventType = normalizeEventType(value.value("event_type", std::string(ids::event::Story)));
    record.eventLabel = value.value("event_label", eventTypeToString(record.eventType));
    record.summary = value.value("summary", std::string {});
    if (record.eventLabel.empty()) {
        record.eventLabel = eventTypeToString(record.eventType);
    }
    return record;
}

BossInfo bossFromJson(const json& value)
{
    BossInfo boss;
    if (!value.is_object()) {
        return boss;
    }

    boss.known = value.value("known", false);
    boss.name = value.value("name", std::string {});
    boss.location = value.value("location", std::string {});
    boss.weakness = value.value("weakness", std::string {});
    boss.description = value.value("description", std::string {});
    boss.known = boss.known || !boss.name.empty() || !boss.location.empty();
    return boss;
}

Prologue prologueFromJson(const json& value)
{
    Prologue prologue;
    if (!value.is_object()) {
        return prologue;
    }

    prologue.generated = value.value("generated", false);
    prologue.text = value.value("text", std::string {});
    prologue.protagonistWound = value.value("protagonist_wound", std::string {});
    prologue.personalGoal = value.value("personal_goal", std::string {});
    prologue.openingLocation = value.value("opening_location", std::string {});
    prologue.firstObjective = value.value(
        "first_clue",
        value.value("first_objective", std::string {}));
    prologue.memoryNote = value.value("memory_note", std::string {});
    prologue.usedFallback = value.value("used_fallback", false);
    prologue.generated = prologue.generated || !prologue.text.empty();
    return prologue;
}

PlayerSnapshot playerFromJson(const json& value)
{
    PlayerSnapshot player;
    if (!value.is_object()) {
        return player;
    }

    player.maxHp = clampInt(value.value("max_hp", player.maxHp), 1, 9999);
    player.hp = clampInt(value.value("hp", player.maxHp), 1, player.maxHp);
    player.level = clampInt(value.value("level", player.level), 1, 999);
    player.attack = clampInt(value.value("attack", player.attack), 1, 999);
    player.defense = clampInt(value.value("defense", player.defense), 0, 999);
    player.gold = std::max(0, value.value("gold", player.gold));
    player.exp = std::max(0, value.value("exp", player.exp));

    if (value.contains("inventory") && value["inventory"].is_array()) {
        player.inventory.clear();
        for (const auto& itemJson : value["inventory"]) {
            if (!itemJson.is_object()) {
                continue;
            }
            const auto item = itemFromJson(itemJson);
            if (!item.name.empty()) {
                player.inventory.push_back(item);
            }
        }
    }
    return player;
}

WorldState worldFromJson(const json& value)
{
    WorldState world;
    if (!value.is_object()) {
        return world;
    }

    world.location = value.value("location", world.location);
    world.currentObjective = value.value("current_objective", world.currentObjective);
    world.decisionHint = value.value("decision_hint", world.decisionHint);
    world.baseCandidate = value.value("base_candidate", false);
    world.fixedRules = stringArrayFromJson(value.value("fixed_rules", json::array()));
    return world;
}

StoryMemory memoryFromJson(const json& value)
{
    StoryMemory memory;
    if (!value.is_object()) {
        return memory;
    }

    memory.recentEvents = stringArrayFromJson(value.value("recent_events", json::array()));
    memory.importantChoices = stringArrayFromJson(value.value("important_choices", json::array()));
    return memory;
}

GameRecords::SnapshotState snapshotFromJson(const json& value)
{
    GameRecords::SnapshotState snapshot;
    if (!value.is_object()) {
        return snapshot;
    }

    snapshot.saved = value.value("saved", false);
    snapshot.turnNumber = clampInt(value.value("turn_number", snapshot.turnNumber), 1, 9999);
    snapshot.currentScene = value.value("current_scene", std::string {});
    snapshot.player = playerFromJson(value.value("player", json::object()));
    snapshot.world = worldFromJson(value.value("world", json::object()));
    snapshot.memory = memoryFromJson(value.value("memory", json::object()));
    snapshot.saved = snapshot.saved || !snapshot.currentScene.empty();
    return snapshot;
}

GameRecords::DangerState dangerFromJson(const json& value)
{
    GameRecords::DangerState danger;
    if (!value.is_object()) {
        return danger;
    }

    danger.level = clampInt(value.value("level", 0), 0, 999);
    danger.lastIncrease = clampInt(value.value("last_increase", 0), 0, 10);
    danger.threshold = clampInt(value.value("threshold", 10), 1, 999);
    return danger;
}

template <typename T, typename Predicate>
void pushUnique(std::vector<T>& values, T value, Predicate same)
{
    if (std::find_if(values.begin(), values.end(), [&](const T& existing) {
            return same(existing, value);
        }) == values.end()) {
        values.push_back(std::move(value));
    }
}

std::vector<std::string> stringArrayFromJson(const json& value)
{
    std::vector<std::string> values;
    if (!value.is_array()) {
        return values;
    }

    for (const auto& entry : value) {
        if (entry.is_string()) {
            const auto text = entry.get<std::string>();
            if (!text.empty()) {
                values.push_back(text);
            }
        }
    }
    return values;
}

json baseToJson(const GameRecords::BaseState& base)
{
    return json {
        {"unlocked", base.unlocked},
        {"location", base.location},
        {"features", base.features},
        {"declined_locations", base.declinedLocations},
    };
}

GameRecords::BaseState baseFromJson(const json& value)
{
    GameRecords::BaseState base;
    if (!value.is_object()) {
        return base;
    }

    base.unlocked = value.value("unlocked", false);
    base.location = value.value("location", std::string {});
    base.features = stringArrayFromJson(value.value("features", json::array()));
    base.declinedLocations = stringArrayFromJson(value.value("declined_locations", json::array()));
    return base;
}

json elderToJson(const GameRecords::ElderState& elder)
{
    return json {
        {"introduced", elder.introduced},
        {"talked", elder.talked},
    };
}

GameRecords::ElderState elderFromJson(const json& value)
{
    GameRecords::ElderState elder;
    if (!value.is_object()) {
        return elder;
    }

    elder.introduced = value.value("introduced", false);
    elder.talked = value.value("talked", false);
    return elder;
}

} // namespace

bool loadGameStateSeedFromJson(const std::string& path, GameState& state, std::string& error)
{
    error.clear();

    try {
        if (path.empty() || !std::filesystem::exists(path)) {
            error = "게임 데이터 파일을 찾을 수 없습니다: " + path;
            return false;
        }

        std::ifstream input(path);
        if (!input) {
            error = "게임 데이터 파일을 열 수 없습니다: " + path;
            return false;
        }

        json root;
        input >> root;
        if (!root.is_object()) {
            error = "게임 데이터 루트가 JSON object가 아닙니다.";
            return false;
        }

        state = {};
        state.turnNumber = clampInt(root.value("turn_number", 1), 1, 9999);
        state.currentScene = root.value("current_scene", std::string {});

        if (root.contains("player") && root["player"].is_object()) {
            state.player = playerFromJson(root["player"]);
        }

        if (root.contains("world") && root["world"].is_object()) {
            const auto fixedRules = stringArrayFromJson(root["world"].value("fixed_rules", json::array()));
            state.world = worldFromJson(root["world"]);
            state.world.fixedRules = fixedRules;
        }

        if (root.contains("starting_items") && root["starting_items"].is_array()) {
            for (const auto& itemJson : root["starting_items"]) {
                if (!itemJson.is_object()) {
                    continue;
                }
                const auto item = itemFromJson(itemJson);
                if (item.name.empty()) {
                    continue;
                }
                state.player.inventory.push_back(item);
                pushUnique(state.records.obtainedItems, item, [](const Item& lhs, const Item& rhs) {
                    return lhs.name == rhs.name;
                });
            }
        }

        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        state = {};
        return false;
    }
}

bool loadGameRecordsFromJson(const std::string& path, GameRecords& records, std::string& error)
{
    error.clear();
    records = {};

    try {
        if (path.empty() || !std::filesystem::exists(path)) {
            return true;
        }

        std::ifstream input(path);
        if (!input) {
            error = "기록 JSON 파일을 열 수 없습니다: " + path;
            return false;
        }

        json root;
        input >> root;

        if (root.contains("quest_log") && root["quest_log"].is_array()) {
            for (const auto& quest : root["quest_log"]) {
                if (quest.is_string()) {
                    const auto text = quest.get<std::string>();
                    if (!text.empty()) {
                        records.questLog.push_back(text);
                    }
                }
            }
        }

        if (root.contains("obtained_items") && root["obtained_items"].is_array()) {
            for (const auto& itemJson : root["obtained_items"]) {
                if (itemJson.is_object()) {
                    const auto item = itemFromJson(itemJson);
                    if (!item.name.empty()) {
                        records.obtainedItems.push_back(item);
                    }
                }
            }
        }

        if (root.contains("encountered_enemies") && root["encountered_enemies"].is_array()) {
            for (const auto& enemyJson : root["encountered_enemies"]) {
                if (enemyJson.is_object()) {
                    const auto enemy = enemyFromJson(enemyJson);
                    if (!enemy.name.empty()) {
                        records.encounteredEnemies.push_back(enemy);
                    }
                }
            }
        }

        if (root.contains("event_history") && root["event_history"].is_array()) {
            for (const auto& eventJson : root["event_history"]) {
                if (eventJson.is_object()) {
                    const auto eventRecord = eventRecordFromJson(eventJson);
                    if (eventRecord.turnNumber > 0 || !eventRecord.summary.empty()) {
                        records.eventHistory.push_back(eventRecord);
                    }
                }
            }
        }

        if (root.contains("snapshot")) {
            records.snapshot = snapshotFromJson(root["snapshot"]);
        }
        if (root.contains("base")) {
            records.base = baseFromJson(root["base"]);
        }
        if (root.contains("prologue")) {
            records.prologue = prologueFromJson(root["prologue"]);
        }
        if (root.contains("danger")) {
            records.danger = dangerFromJson(root["danger"]);
        }
        if (root.contains("elder")) {
            records.elder = elderFromJson(root["elder"]);
        }
        if (root.contains("boss")) {
            records.boss = bossFromJson(root["boss"]);
        }

        GameRecords deduped;
        mergeGameRecords(deduped, records);
        records = std::move(deduped);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        records = {};
        return false;
    }
}

bool saveGameRecordsToJson(const std::string& path, const GameRecords& records, std::string& error)
{
    error.clear();

    try {
        if (path.empty()) {
            error = "기록 JSON 경로가 비어 있습니다.";
            return false;
        }

        const std::filesystem::path dbPath(path);
        if (dbPath.has_parent_path()) {
            std::filesystem::create_directories(dbPath.parent_path());
        }

        json root;
        root["version"] = 1;
        root["snapshot"] = snapshotToJson(records.snapshot);
        root["quest_log"] = records.questLog;
        root["obtained_items"] = json::array();
        root["encountered_enemies"] = json::array();
        root["event_history"] = json::array();
        root["prologue"] = prologueToJson(records.prologue);
        root["danger"] = dangerToJson(records.danger);
        root["base"] = baseToJson(records.base);
        root["elder"] = elderToJson(records.elder);
        root["boss"] = bossToJson(records.boss);

        for (const auto& item : records.obtainedItems) {
            root["obtained_items"].push_back(itemToJson(item));
        }
        for (const auto& enemy : records.encounteredEnemies) {
            root["encountered_enemies"].push_back(enemyToJson(enemy));
        }
        for (const auto& eventRecord : records.eventHistory) {
            root["event_history"].push_back(eventRecordToJson(eventRecord));
        }

        std::ofstream output(path);
        if (!output) {
            error = "기록 JSON 파일을 쓸 수 없습니다: " + path;
            return false;
        }

        output << root.dump(2) << '\n';
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

void mergeGameRecords(GameRecords& target, const GameRecords& source)
{
    if (source.snapshot.saved) {
        target.snapshot = source.snapshot;
    }

    for (const auto& quest : source.questLog) {
        if (!quest.empty()
            && std::find(target.questLog.begin(), target.questLog.end(), quest) == target.questLog.end()) {
            target.questLog.push_back(quest);
        }
    }

    for (const auto& item : source.obtainedItems) {
        if (!item.name.empty()) {
            pushUnique(target.obtainedItems, item, [](const Item& lhs, const Item& rhs) {
                return lhs.name == rhs.name;
            });
        }
    }

    for (const auto& enemy : source.encounteredEnemies) {
        if (!enemy.name.empty()) {
            pushUnique(target.encounteredEnemies, enemy, [](const Monster& lhs, const Monster& rhs) {
                return lhs.name == rhs.name;
            });
        }
    }

    for (const auto& eventRecord : source.eventHistory) {
        if (eventRecord.turnNumber > 0 || !eventRecord.summary.empty()) {
            pushUnique(target.eventHistory, eventRecord, [](const GameRecords::EventRecord& lhs, const GameRecords::EventRecord& rhs) {
                return lhs.turnNumber == rhs.turnNumber
                    && lhs.eventType == rhs.eventType
                    && lhs.summary == rhs.summary;
            });
        }
    }

    if (source.prologue.generated) {
        target.prologue = source.prologue;
    }
    if (source.danger.level > 0 || source.danger.lastIncrease > 0 || source.danger.threshold != 10) {
        target.danger = source.danger;
    }

    if (source.base.unlocked) {
        target.base.unlocked = true;
        if (!source.base.location.empty()) {
            target.base.location = source.base.location;
        }
    }
    for (const auto& feature : source.base.features) {
        if (!feature.empty()
            && std::find(target.base.features.begin(), target.base.features.end(), feature) == target.base.features.end()) {
            target.base.features.push_back(feature);
        }
    }
    for (const auto& location : source.base.declinedLocations) {
        if (!location.empty()
            && std::find(target.base.declinedLocations.begin(), target.base.declinedLocations.end(), location)
                == target.base.declinedLocations.end()) {
            target.base.declinedLocations.push_back(location);
        }
    }

    if (source.elder.introduced) {
        target.elder.introduced = true;
    }
    if (source.elder.talked) {
        target.elder.talked = true;
    }
    if (source.boss.known) {
        target.boss = source.boss;
    }
}

} // namespace textrpg::llm::internals
