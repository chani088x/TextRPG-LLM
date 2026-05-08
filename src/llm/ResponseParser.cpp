#include "llm/ResponseParser.hpp"
#include "llm/LLMEngineInternals.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <iostream>

namespace textrpg::llm::internals {
namespace {

class ResponseRepairer {
public:
    static void repairEvent(GameEvent& event)
    {
        if (event.sceneText.empty()) {
            event.sceneText = "당신은 주변을 살피며 다음 행동을 고민한다.";
            event.validationNotes.push_back("scene_text was empty");
        }
        if (event.memoryNote.empty()) {
            event.memoryNote = "장면이 이어졌다.";
            event.validationNotes.push_back("memory_note was empty");
        }
        if (event.nextObjective.empty()) {
            event.nextObjective = "현재 장면의 단서를 확인하고 다음 행동을 정한다";
            event.validationNotes.push_back("next_objective was empty");
        }
        if (event.decisionHint.empty()) {
            event.decisionHint = "정보 확보와 안전 확보 중 무엇을 우선할지 판단해야 한다";
            event.validationNotes.push_back("decision_hint was empty");
        }
        if (event.eventType == ids::event::Combat && !event.monster.has_value()) {
            event.monster = Monster {"몬스터", "갑자기 모습을 드러낸 적.", 10, 1, 0};
            event.validationNotes.push_back("combat monster was missing");
        }
        if (event.eventType != ids::event::Combat
            && event.eventType != ids::event::GameEnd
            && event.choices.size() < 2) {
            event.choices = {
                "눈앞의 단서를 자세히 조사한다",
                "시간을 아끼기 위해 목표 방향으로 전진한다",
            };
            event.validationNotes.push_back("choices were repaired");
        }

        event.statChanges.hp = clampInt(event.statChanges.hp, -30, 30);
        event.statChanges.gold = clampInt(event.statChanges.gold, 0, 100);
        event.statChanges.exp = clampInt(event.statChanges.exp, 0, 50);
    }

    static void repairActionResult(ActionResult& result, const std::string& diceOutcome)
    {
        const auto normalizedOutcome = normalizeDiceOutcome(diceOutcome);

        if (result.resultText.empty()) {
            result.resultText = "고유 행동은 짧은 여파를 남겼다.";
            result.notes.push_back("result_text was empty");
        }
        if (result.resultType.empty()) {
            result.resultType = "nothing";
            result.notes.push_back("result_type was empty");
        }

        if (isFailureOutcome(normalizedOutcome)) {
            result.goldDelta = 0;
            result.expDelta = 0;
            result.itemName.clear();
            result.itemDescription.clear();
            if (result.hpDelta > 0) {
                result.hpDelta = 0;
            }
            result.notes.push_back("failure rewards were removed");
            return;
        }

        if (result.goldDelta < 0) {
            result.goldDelta = 0;
            result.notes.push_back("negative gold was removed");
        }
        if (result.expDelta < 0) {
            result.expDelta = 0;
            result.notes.push_back("negative exp was removed");
        }
    }

    static void repairInitialWorld(InitialWorld& world)
    {
        if (world.location.empty()) {
            world.location = "이름 없는 길";
            world.notes.push_back("location was empty");
        }
        if (world.sceneText.empty()) {
            world.sceneText = "낯선 길 위에서 첫 여정이 시작된다.";
            world.notes.push_back("scene_text was empty");
        }
        if (world.currentObjective.empty()) {
            world.currentObjective = "현재 장소의 단서를 확인한다";
            world.notes.push_back("current_objective was empty");
        }
        if (world.decisionHint.empty()) {
            world.decisionHint = "정보를 모을지, 빠르게 이동할지 판단해야 한다";
            world.notes.push_back("decision_hint was empty");
        }
        if (world.memoryNote.empty()) {
            world.memoryNote = "첫 장면이 생성되었다.";
            world.notes.push_back("memory_note was empty");
        }
    }
};

bool extractJsonObject(const std::string& rawText, std::string& jsonText)
{
    const auto start = rawText.find('{');
    if (start == std::string::npos) {
        return false;
    }

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (std::size_t i = start; i < rawText.size(); ++i) {
        const char ch = rawText[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                jsonText = rawText.substr(start, i - start + 1);
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> readChoices(const nlohmann::json& root, const std::string& eventType)
{
    if (eventType == ids::event::Combat || !root.contains("choices") || !root["choices"].is_array()) {
        return {};
    }

    std::vector<std::string> choices;
    for (const auto& choice : root["choices"]) {
        if (choice.is_string() && !choice.get<std::string>().empty()) {
            choices.push_back(choice.get<std::string>());
        }
    }
    if (choices.size() > 5) {
        choices.resize(5);
    }
    return choices;
}

std::optional<Monster> readMonster(const nlohmann::json& root, const std::string& eventType)
{
    if (eventType != ids::event::Combat || !root.contains("monster") || !root["monster"].is_object()) {
        return std::nullopt;
    }

    const auto& monster = root["monster"];
    return Monster {
        monster.value("name", "몬스터"),
        monster.value("description", ""),
        10,
        1,
        0,
    };
}

std::optional<Item> readItem(const nlohmann::json& root, std::vector<std::string>& notes)
{
    if (!root.contains("item") || !root["item"].is_object()) {
        return std::nullopt;
    }

    const auto& item = root["item"];
    const auto rawType = item.value("type", std::string(ids::item::Consumable));
    const auto normalizedType = normalizeItemType(rawType);
    if (rawType != normalizedType) {
        notes.push_back("item type was invalid and repaired");
    }

    return Item {
        item.value("name", "알 수 없는 물건"),
        normalizedType,
        item.value("description", ""),
        clampInt(item.value("value", 0), 0, 100),
    };
}

} // namespace

GameEvent fallbackEvent(const std::string& reason, const std::string& eventType)
{
    GameEvent event;
    event.eventType = normalizeEventType(eventType);
    if (event.eventType == ids::event::Combat) {
        event.sceneText = "어둠 속에서 적의 기척이 가까워진다. 당신은 무기를 고쳐 쥐고 전투 태세를 갖춘다.";
        event.nextObjective = "눈앞의 적을 쓰러뜨리고 다음 길을 확보한다";
        event.decisionHint = "기본 공격으로 빠르게 제압할지, 고유 행동으로 전투 상황을 바꿀지 판단해야 한다";
        event.monster = Monster {"그림자 약탈자", "LLM 응답 오류 중에도 전투 흐름을 유지하기 위해 등장한 적.", 10, 1, 0};
        event.memoryNote = "LLM 응답 오류로 안전한 기본 전투 이벤트가 생성되었다.";
    } else {
        event.eventType = ids::event::Story;
        event.sceneText = "잠시 주변이 조용해졌다. 당신은 숨을 고르며 다음 행동을 정리한다.";
        event.nextObjective = "주변 상황을 다시 확인하고 안전한 단서를 찾는다";
        event.decisionHint = "정보를 더 모으면 안전하지만 시간이 지나며 단서가 사라질 수 있다";
        event.choices = {
            "흔적이 남은 바닥과 주변 표식을 자세히 조사한다",
            "소리를 낮추고 안전한 우회로를 찾아 이동한다",
        };
        event.memoryNote = "LLM 응답 오류로 안전한 기본 이벤트가 생성되었다.";
    }
    event.usedFallback = true;
    event.validationNotes.push_back(reason);
    return event;
}

ActionResult fallbackActionResult(const std::string& reason, const std::string& diceOutcome)
{
    const auto normalizedOutcome = normalizeDiceOutcome(diceOutcome);

    ActionResult result;
    result.resultText = "고유 행동은 뚜렷한 결과를 만들지 못했다.";
    result.resultType = "nothing";
    if (isFailureOutcome(normalizedOutcome)) {
        result.resultText = "고유 행동이 빗나가며 상황이 조금 불리해졌다.";
        result.resultType = "damage";
        result.hpDelta = -1;
    }
    result.usedFallback = true;
    result.notes.push_back(reason);
    return result;
}

InitialWorld fallbackInitialWorld(const std::string& reason)
{
    InitialWorld world;
    world.location = "이름 없는 길";
    world.sceneText = "낯선 길 위로 오래된 표식과 희미한 발자국이 이어진다.";
    world.currentObjective = "낯선 표식의 의미를 확인한다";
    world.decisionHint = "표식을 조사하면 단서를 얻지만, 이동하면 더 빨리 다음 장소에 닿을 수 있다";
    world.memoryNote = "LLM 시작 상황 생성 실패로 기본 시작 상황이 생성되었다.";
    world.usedFallback = true;
    world.notes.push_back(reason);
    return world;
}

GameEvent ResponseParser::parseEvent(const std::string& rawText, const std::string& fallbackType)
{
    std::string jsonText;
    if (!extractJsonObject(rawText, jsonText)) {
        return fallbackEvent("no JSON object found", fallbackType);
    }

    try {
        const auto root = nlohmann::json::parse(jsonText);
        if (!root.is_object()) {
            return fallbackEvent("JSON root is not an object", fallbackType);
        }

        GameEvent event;
        event.sceneText = root.value("scene_text", "");
        event.location = root.value("location", "");

        const auto requestedType = root.value("event_type", std::string(ids::event::Story));
        const bool restrictToFallbackType = isKnownEventType(fallbackType);
        if (!isKnownEventType(requestedType)) {
            event.eventType = restrictToFallbackType ? fallbackType : ids::event::Story;
            event.validationNotes.push_back("event_type was invalid and repaired");
        } else if (restrictToFallbackType && requestedType != fallbackType) {
            event.eventType = fallbackType;
            event.validationNotes.push_back("event_type was repaired to requested content type");
        } else {
            event.eventType = requestedType;
        }

        event.nextObjective = root.value("next_objective", "");
        event.decisionHint = root.value("decision_hint", "");
        event.choices = readChoices(root, event.eventType);
        event.monster = readMonster(root, event.eventType);
        event.item = readItem(root, event.validationNotes);

        if (root.contains("stat_changes") && root["stat_changes"].is_object()) {
            const auto& stats = root["stat_changes"];
            event.statChanges.hp = stats.value("hp", 0);
            event.statChanges.gold = stats.value("gold", 0);
            event.statChanges.exp = stats.value("exp", 0);
        }

        event.memoryNote = root.value("memory_note", "");
        ResponseRepairer::repairEvent(event);
        return event;
    } catch (const std::exception& ex) {
        return fallbackEvent(ex.what(), fallbackType);
    }
}

ActionResult ResponseParser::parseActionResult(const std::string& rawText, const std::string& diceOutcome)
{    
    std::string jsonText;
    if (!extractJsonObject(rawText, jsonText)) {
        return fallbackActionResult("no JSON object found", diceOutcome);
    }

    try {
        const auto root = nlohmann::json::parse(jsonText);
        if (!root.is_object()) {
            return fallbackActionResult("JSON root is not an object", diceOutcome);
        }

        ActionResult result;
        result.resultText = root.value("result_text", "");
        result.resultType = root.value("result_type", "");
        result.itemName = root.value("item_name", "");
        result.itemDescription = root.value("item_description", "");
        result.hpDelta = root.value("hp_delta", 0);
        result.goldDelta = root.value("gold_delta", 0);
        result.expDelta = root.value("exp_delta", 0);
        ResponseRepairer::repairActionResult(result, diceOutcome);
        return result;
    } catch (const std::exception& ex) {
        return fallbackActionResult(ex.what(), diceOutcome);
    }
}

InitialWorld ResponseParser::parseInitialWorld(const std::string& rawText)
{
    std::string jsonText;
    if (!extractJsonObject(rawText, jsonText)) {
        return fallbackInitialWorld("no JSON object found");
    }

    try {
        const auto root = nlohmann::json::parse(jsonText);
        if (!root.is_object()) {
            return fallbackInitialWorld("JSON root is not an object");
        }

        InitialWorld world;
        world.location = root.value("location", "");
        world.sceneText = root.value("scene_text", "");
        world.currentObjective = root.value("current_objective", "");
        world.decisionHint = root.value("decision_hint", "");
        world.memoryNote = root.value("memory_note", "");
        ResponseRepairer::repairInitialWorld(world);
        return world;
    } catch (const std::exception& ex) {
        return fallbackInitialWorld(ex.what());
    }
}

GameEvent parseEvent(const std::string& rawText, const std::string& fallbackType)
{
    return ResponseParser::parseEvent(rawText, fallbackType);
}

ActionResult parseActionResult(const std::string& rawText, const std::string& diceOutcome)
{
    return ResponseParser::parseActionResult(rawText, diceOutcome);
}

InitialWorld parseInitialWorld(const std::string& rawText)
{
    return ResponseParser::parseInitialWorld(rawText);
}

} // namespace textrpg::llm::internals
