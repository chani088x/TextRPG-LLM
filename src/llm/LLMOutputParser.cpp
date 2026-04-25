#include "llm/LLMOutputParser.hpp"

#include <cstddef>
#include <exception>

namespace textrpg::llm {
namespace {

bool requireField(const nlohmann::json& object, const char* key, nlohmann::json::value_t type, std::string& error)
{
    if (!object.contains(key)) {
        error = std::string("missing required field: ") + key;
        return false;
    }
    if (object.at(key).type() != type) {
        error = std::string("invalid field type: ") + key;
        return false;
    }
    return true;
}

bool requireIntField(const nlohmann::json& object, const char* key, std::string& error)
{
    if (!object.contains(key)) {
        error = std::string("missing required field: ") + key;
        return false;
    }
    if (!object.at(key).is_number_integer()) {
        error = std::string("invalid integer field: ") + key;
        return false;
    }
    return true;
}

bool readOptionalIntField(const nlohmann::json& object, const char* key, int& output, std::string& error)
{
    if (!object.contains(key)) {
        output = 0;
        return true;
    }
    if (!object.at(key).is_number_integer()) {
        error = std::string("invalid integer field: ") + key;
        return false;
    }
    output = object.at(key).get<int>();
    return true;
}

} // namespace

ParseResult LLMOutputParser::parse(const std::string& rawText) const
{
    ParseResult result;
    std::string jsonText;
    // 모델이 앞뒤에 설명문이나 코드펜스를 붙여도 첫 JSON 객체만 꺼내본다.
    if (!extractJsonObject(rawText, jsonText, result.errorMessage)) {
        return result;
    }

    try {
        result.extractedJson = nlohmann::json::parse(jsonText);
    } catch (const std::exception& ex) {
        result.errorMessage = std::string("json parse failed: ") + ex.what();
        return result;
    }

    const auto& root = result.extractedJson;
    if (!root.is_object()) {
        result.errorMessage = "root JSON value is not an object";
        return result;
    }

    // 필수 필드가 빠지면 GameEvent 후보를 만들지 않고 fallback 흐름으로 넘긴다.
    if (!requireField(root, "scene_text", nlohmann::json::value_t::string, result.errorMessage)
        || !requireField(root, "event_type", nlohmann::json::value_t::string, result.errorMessage)
        || !requireField(root, "stat_changes", nlohmann::json::value_t::object, result.errorMessage)
        || !requireField(root, "memory_note", nlohmann::json::value_t::string, result.errorMessage)) {
        return result;
    }

    GameEvent event;
    event.sceneText = root.at("scene_text").get<std::string>();
    event.eventType = eventTypeFromString(root.at("event_type").get<std::string>());
    if (root.contains("next_objective")) {
        if (!root.at("next_objective").is_string()) {
            result.errorMessage = "invalid field type: next_objective";
            return result;
        }
        event.nextObjective = root.at("next_objective").get<std::string>();
    }
    if (root.contains("decision_hint")) {
        if (!root.at("decision_hint").is_string()) {
            result.errorMessage = "invalid field type: decision_hint";
            return result;
        }
        event.decisionHint = root.at("decision_hint").get<std::string>();
    }
    // 허용 목록 밖의 문자열은 여기서 차단해 Validator가 Invalid 이벤트를 다루지 않게 한다.
    if (event.eventType == EventType::Invalid) {
        result.errorMessage = "event_type is not allowed: " + root.at("event_type").get<std::string>();
        return result;
    }

    if (root.contains("choices")) {
        if (!root.at("choices").is_array()) {
            result.errorMessage = "invalid field type: choices";
            return result;
        }
        for (const auto& choice : root.at("choices")) {
            if (!choice.is_string()) {
                result.errorMessage = "choices must contain only strings";
                return result;
            }
            event.choices.push_back(choice.get<std::string>());
        }
    }

    if (root.contains("monster")) {
        if (!parseMonster(root.at("monster"), event.monster, result.errorMessage)) {
            return result;
        }
    }

    if (root.contains("item")) {
        if (!parseItem(root.at("item"), event.item, result.errorMessage)) {
            return result;
        }
    }

    const auto& statChanges = root.at("stat_changes");
    if (!readOptionalIntField(statChanges, "hp", event.statChanges.hp, result.errorMessage)
        || !readOptionalIntField(statChanges, "gold", event.statChanges.gold, result.errorMessage)
        || !readOptionalIntField(statChanges, "exp", event.statChanges.exp, result.errorMessage)) {
        return result;
    }
    event.memoryNote = root.at("memory_note").get<std::string>();

    result.success = true;
    result.event = std::move(event);
    return result;
}

bool LLMOutputParser::extractJsonObject(const std::string& rawText, std::string& jsonText, std::string& errorMessage)
{
    const auto start = rawText.find('{');
    if (start == std::string::npos) {
        errorMessage = "no JSON object start found";
        return false;
    }

    bool inString = false;
    bool escaped = false;
    int depth = 0;

    // 문자열 내부의 중괄호는 JSON 구조 깊이로 세지 않기 위해 따로 추적한다.
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

    errorMessage = "no matching JSON object end found";
    return false;
}

bool LLMOutputParser::parseMonster(
    const nlohmann::json& value,
    std::optional<Monster>& monster,
    std::string& errorMessage)
{
    // monster는 combat이 아닐 때 null일 수 있다. 이벤트 타입과의 관계는 Validator가 확인한다.
    if (value.is_null()) {
        monster.reset();
        return true;
    }
    if (!value.is_object()) {
        errorMessage = "monster must be null or object";
        return false;
    }

    if (!requireField(value, "name", nlohmann::json::value_t::string, errorMessage)
        || !requireField(value, "description", nlohmann::json::value_t::string, errorMessage)
        || !requireIntField(value, "hp", errorMessage)
        || !requireIntField(value, "attack", errorMessage)
        || !requireIntField(value, "defense", errorMessage)) {
        return false;
    }

    monster = Monster {
        value.at("name").get<std::string>(),
        value.at("description").get<std::string>(),
        value.at("hp").get<int>(),
        value.at("attack").get<int>(),
        value.at("defense").get<int>(),
    };
    return true;
}

bool LLMOutputParser::parseItem(
    const nlohmann::json& value,
    std::optional<Item>& item,
    std::string& errorMessage)
{
    // item도 선택 필드지만, item_gain에서 비어 있으면 Validator가 실패 처리한다.
    if (value.is_null()) {
        item.reset();
        return true;
    }
    if (!value.is_object()) {
        errorMessage = "item must be null or object";
        return false;
    }

    if (!requireField(value, "name", nlohmann::json::value_t::string, errorMessage)
        || !requireField(value, "type", nlohmann::json::value_t::string, errorMessage)
        || !requireField(value, "description", nlohmann::json::value_t::string, errorMessage)
        || !requireIntField(value, "value", errorMessage)) {
        return false;
    }

    const auto itemType = itemTypeFromString(value.at("type").get<std::string>());
    if (itemType == ItemType::Invalid) {
        errorMessage = "item.type is not allowed: " + value.at("type").get<std::string>();
        return false;
    }

    item = Item {
        value.at("name").get<std::string>(),
        itemType,
        value.at("description").get<std::string>(),
        value.at("value").get<int>(),
    };
    return true;
}

} // namespace textrpg::llm
