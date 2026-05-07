#include "llm/LLM.hpp"

#include <ollama.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <sstream>
#include <utility>

namespace textrpg::llm {
namespace {

std::string joinLimited(const std::vector<std::string>& values, std::size_t maxItems, const std::string& emptyText)
{
    if (values.empty()) {
        return "- " + emptyText + "\n";
    }

    std::ostringstream out;
    const auto start = values.size() > maxItems ? values.size() - maxItems : 0;
    for (std::size_t i = start; i < values.size(); ++i) {
        out << "- " << values[i] << '\n';
    }
    return out.str();
}

std::string buildPrompt(const GameState& state, const std::string& playerInput)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 게임 마스터다.\n";
    out << "플레이어 행동의 결과를 먼저 묘사하고 다음 상황을 만든다.\n";
    out << "새 적이 처음 등장할 때만 event_type을 combat으로 둔다.\n";
    out << "이미 등장한 적과 싸우는 입력이면 새 combat을 반복하지 말고 전투 직후 결과를 story나 quest_update로 묘사한다.\n";
    out << "늑대 같은 흔한 적을 반복하지 말고 장소, 단서, 인물, 장애물을 다양하게 사용한다.\n";
    out << "next_objective와 decision_hint는 이전 목표/판단 기준을 그대로 반복하지 않는다.\n";
    out << "combat의 monster 수치는 무시되므로 name과 description만 중요하다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";

    //NPC 관련 지침 추가
    out << "새로운 장소나 상황에서 NPC가 필요하다면 즉석에서 NPC를 추가해도 된다.\n";
    out << "NPC 생성 시 이름, 성격, 말투를 정하여 new_npc 필드로 반환한다. \n";
    out << "이미 존재하는 인물이거나 NPC가 필요없는 상황에서는 new_npc를 null로 둔다.\n";

    out << "[현재 상태]\n";
    out << "턴: " << state.turnNumber << '\n';
    out << "위치: " << state.world.location << '\n';
    out << "현재 장면: " << state.currentScene << '\n';
    out << "현재 목표: " << state.world.currentObjective << '\n';
    out << "현재 판단 기준: " << state.world.decisionHint << '\n';
    out << "플레이어 HP/ATK: " << state.player.hp << "/" << state.player.attack << '\n';
    out << "인벤토리:\n" << joinLimited(state.player.inventory, 8, "없음");
    out << "최근 사건:\n" << joinLimited(state.memory.recentEvents, 5, "없음");
    out << "중요 선택:\n" << joinLimited(state.memory.importantChoices, 5, "없음");
    out << "\n[플레이어 입력]\n" << playerInput << "\n\n";

    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"scene_text\": \"string\",\n";
    out << "  \"event_type\": \"story | combat | item_gain | stat_change | dialogue | quest_update | rest | game_end\",\n";
    out << "  \"next_objective\": \"string\",\n";
    out << "  \"decision_hint\": \"string\",\n";
    out << "  \"choices\": [\"string\", \"string\"],\n";

    // npc 생성 지침 추가
    out << "  \"new_npc\": {\n";
    out << "  \"name\": \"string\",\n";
    out << "  \"personality\": \"string\",\n";
    out << "  \"speech_style\": \"string\"\n";
    out << "   },\n";

    out << "  \"monster\": null,\n";
    out << "  \"item\": null,\n";
    out << "  \"stat_changes\": {\"hp\": 0, \"gold\": 0, \"exp\": 0},\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    out << "combat이면 monster는 {\"name\":\"string\",\"description\":\"string\"} 형태로 채운다.\n";
    out << "combat이면 choices는 []로 둔다. 전투 행동은 C++ 전투 시스템이 처리한다.\n";
    return out.str();
}

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

GameEvent fallbackEvent(const std::string& reason)
{
    GameEvent event;
    event.sceneText = "잠시 주변이 조용해졌다. 당신은 숨을 고르며 다음 행동을 정리한다.";
    event.eventType = EventType::Story;
    event.nextObjective = "주변 상황을 다시 확인하고 안전한 단서를 찾는다";
    event.decisionHint = "정보를 더 모으면 안전하지만 시간이 지나며 단서가 사라질 수 있다";
    event.choices = {
        "흔적이 남은 바닥과 주변 표식을 자세히 조사한다",
        "소리를 낮추고 안전한 우회로를 찾아 이동한다",
        "잠시 숨을 고르며 장비와 부상 상태를 확인한다",
    };
    event.memoryNote = "LLM 응답 오류로 안전한 기본 이벤트가 생성되었다.";
    event.usedFallback = true;
    event.validationNotes.push_back(reason);
    return event;
}

std::vector<std::string> readChoices(const nlohmann::json& root, EventType eventType)
{
    if (eventType == EventType::Combat || !root.contains("choices") || !root["choices"].is_array()) {
        return {};
    }

    std::vector<std::string> choices;
    for (const auto& choice : root["choices"]) {
        if (choice.is_string() && !choice.get<std::string>().empty()) {
            choices.push_back(choice.get<std::string>());
        }
    }
    if (choices.size() > 4) {
        choices.resize(4);
    }
    return choices;
}

std::optional<Monster> readMonster(const nlohmann::json& root, EventType eventType)
{
    if (eventType != EventType::Combat || !root.contains("monster") || !root["monster"].is_object()) {
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

std::optional<Item> readItem(const nlohmann::json& root)
{
    if (!root.contains("item") || !root["item"].is_object()) {
        return std::nullopt;
    }

    const auto& item = root["item"];
    return Item {
        item.value("name", "알 수 없는 물건"),
        itemTypeFromString(item.value("type", "consumable")),
        item.value("description", ""),
        clampInt(item.value("value", 0), 0, 100),
    };
}



// NPC
std::optional<GeneratedNPC> readGeneratedNPC(const nlohmann::json& root)
{
    if (!root.contains("new_npc") || !root["new_npc"].is_null() || !root["new_npc"].is_object()) {
        return std::nullopt;
    }

    const auto& npc = root["new_npc"];
    GeneratedNPC result;

    result.name = npc.value("name", "알 수 없는 인물");
    result.personality = npc.value("personality", "성격이 드러나지 않음");
    result.speechStyle = npc.value("speech_style", "말투가 드러나지 않음");
    result.affinity = 0;
    result.isMet = true;

    return result;
}

void repairEvent(GameEvent& event)
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
    if (event.eventType == EventType::Combat && !event.monster.has_value()) {
        event.monster = Monster {"몬스터", "갑자기 모습을 드러낸 적.", 10, 1, 0};
        event.validationNotes.push_back("combat monster was missing");
    }
    if (event.eventType != EventType::Combat && event.choices.size() < 2) {
        event.choices = {
            "눈앞의 단서를 자세히 조사한다",
            "안전을 우선하며 다른 경로를 확인한다",
            "시간을 아끼기 위해 목표 방향으로 전진한다",
        };
        event.validationNotes.push_back("choices were repaired");
    }

    event.statChanges.hp = clampInt(event.statChanges.hp, -30, 30);
    event.statChanges.gold = clampInt(event.statChanges.gold, 0, 100);
    event.statChanges.exp = clampInt(event.statChanges.exp, 0, 50);
}

} // namespace

LLM::LLM(LLMOptions options)
    : options_(std::move(options))
{
}

GameEvent LLM::generateEvent(const GameState& state, const std::string& playerInput) const
{
    try {
        Ollama client(options_.endpoint);
        client.setConnectionTimeout(options_.connectionTimeoutSeconds);
        client.setReadTimeout(options_.readTimeoutSeconds);
        client.setWriteTimeout(options_.readTimeoutSeconds);

        ollama::request request(ollama::message_type::chat);
        request["model"] = options_.model;
        request["messages"] = nlohmann::json::array({
            {{"role", "user"}, {"content", buildPrompt(state, playerInput)}}
        });
        request["stream"] = false;
        request["think"] = options_.think;
        request["keep_alive"] = "5m";
        request["options"] = {{"temperature", options_.temperature}};

        const ollama::response response = client.chat(request);
        return parseEvent(response.as_simple_string());
    } catch (const std::exception& ex) {
        return fallbackEvent(ex.what());
    }
}

GameEvent LLM::parseEvent(const std::string& rawText)
{
    std::string jsonText;
    if (!extractJsonObject(rawText, jsonText)) {
        return fallbackEvent("no JSON object found");
    }

    try {
        const auto root = nlohmann::json::parse(jsonText);
        if (!root.is_object()) {
            return fallbackEvent("JSON root is not an object");
        }

        GameEvent event;
        event.sceneText = root.value("scene_text", "");
        event.eventType = eventTypeFromString(root.value("event_type", "story"));
        if (event.eventType == EventType::Invalid) {
            event.eventType = EventType::Story;
            event.validationNotes.push_back("event_type was invalid");
        }
        event.nextObjective = root.value("next_objective", "");
        event.decisionHint = root.value("decision_hint", "");
        event.choices = readChoices(root, event.eventType);
        event.monster = readMonster(root, event.eventType);
        event.item = readItem(root);
        // NPC 읽기 추가
        event.newNPC = readGeneratedNPC(root);

        if (root.contains("stat_changes") && root["stat_changes"].is_object()) {
            const auto& stats = root["stat_changes"];
            event.statChanges.hp = stats.value("hp", 0);
            event.statChanges.gold = stats.value("gold", 0);
            event.statChanges.exp = stats.value("exp", 0);
        }

        event.memoryNote = root.value("memory_note", "");
        repairEvent(event);
        return event;
    } catch (const std::exception& ex) {
        return fallbackEvent(ex.what());
    }
}

} // namespace textrpg::llm
