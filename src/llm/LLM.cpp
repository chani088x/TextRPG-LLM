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

std::string baseContext(const GameState& state)
{
    std::ostringstream out;
    out << "[현재 상태]\n";
    out << "턴: " << state.turnNumber << '\n';
    out << "위치: " << state.world.location << '\n';
    out << "현재 장면: " << state.currentScene << '\n';
    out << "현재 목표: " << state.world.currentObjective << '\n';
    out << "현재 판단 기준: " << state.world.decisionHint << '\n';
    out << "플레이어 HP/ATK: " << state.player.hp << "/" << state.player.attack << '\n';
    out << "Gold/Exp: " << state.player.gold << "/" << state.player.exp << '\n';
    out << "인벤토리:\n" << joinLimited(state.player.inventory, 8, "없음");
    out << "최근 사건:\n" << joinLimited(state.memory.recentEvents, 5, "없음");
    out << "중요 선택:\n" << joinLimited(state.memory.importantChoices, 5, "없음");
    return out.str();
}

std::string buildCombatPrompt(const GameState& state, const std::string& actionContext)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 전투 장면 생성기다.\n";
    out << "content_type은 combat이다. 새 전투 상황과 몬스터 이름/설명을 만든다.\n";
    out << "몬스터 수치는 C++ 전투 시스템이 정하므로 name과 description만 중요하다.\n";
    out << "플레이어가 방금 한 행동 결과를 첫 문장에 반영한다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << baseContext(state) << '\n';
    out << "[직전 행동 컨텍스트]\n" << actionContext << "\n\n";
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"scene_text\": \"string\",\n";
    out << "  \"location\": \"string\",\n";
    out << "  \"event_type\": \"combat\",\n";
    out << "  \"next_objective\": \"string\",\n";
    out << "  \"decision_hint\": \"string\",\n";
    out << "  \"choices\": [],\n";
    out << "  \"monster\": {\"name\": \"string\", \"description\": \"string\"},\n";
    out << "  \"item\": null,\n";
    out << "  \"stat_changes\": {\"hp\": 0, \"gold\": 0, \"exp\": 0},\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    return out.str();
}

std::string buildStoryPrompt(const GameState& state, const std::string& actionContext)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 비전투 스토리 장면 생성기다.\n";
    out << "content_type은 story다. 장면은 기, 승, 전, 결의 흐름 중 자연스러운 한 구간처럼 이어진다.\n";
    out << "플레이어가 방금 한 행동 결과를 첫 문장에 반영한다.\n";
    out << "전투는 만들지 말고 탐험, 단서, 분위기, 인물 반응을 중심으로 쓴다.\n";
    out << "선택지는 최대 5개까지 가능하지만, 기본 선택지는 C++ 코드가 제공하므로 0~2개만 제안한다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << baseContext(state) << '\n';
    out << "[직전 행동 컨텍스트]\n" << actionContext << "\n\n";
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"scene_text\": \"string\",\n";
    out << "  \"location\": \"string\",\n";
    out << "  \"event_type\": \"story\",\n";
    out << "  \"next_objective\": \"string\",\n";
    out << "  \"decision_hint\": \"string\",\n";
    out << "  \"choices\": [\"string\"],\n";
    out << "  \"monster\": null,\n";
    out << "  \"item\": null,\n";
    out << "  \"stat_changes\": {\"hp\": 0, \"gold\": 0, \"exp\": 0},\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    return out.str();
}

std::string buildNextEventPrompt(const GameState& state, const std::string& actionContext)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 다음 장면 디렉터다.\n";
    out << "현재 상태, 최근 사건, 플레이어의 직전 행동을 보고 다음 event_type을 직접 고른다.\n";
    out << "전투 여부를 랜덤처럼 고르지 말고, 직전 행동의 결과로 자연스럽게 이어지는 장면을 만든다.\n";
    out << "전투 계산과 보상 확정은 C++ 엔진이 담당하므로 수치 결과를 과장하지 않는다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << "[event_type 선택 규칙]\n";
    out << "- story: 이동, 탐험, 단서 발견, 분위기 변화처럼 일반 진행이 자연스러울 때.\n";
    out << "- combat: 적과 직접 충돌했거나, 길이 막혔거나, 플레이어 행동이 위협을 건드렸을 때.\n";
    out << "- item_gain: 행동 결과로 물건을 실제로 얻는 장면일 때.\n";
    out << "- stat_change: 피해, 회복, 피로, 환경 효과처럼 상태 변화가 핵심일 때.\n";
    out << "- dialogue: 인물과의 대화나 협상이 핵심일 때.\n";
    out << "- quest_update: 목표, 임무, 단서의 의미가 바뀔 때.\n";
    out << "- rest: 안전한 휴식이나 정비가 자연스러울 때.\n";
    out << "- game_end: 명확한 결말 조건이 충족되었을 때만.\n\n";
    out << "[진행 규칙]\n";
    out << "- 조사 행동은 보통 story, dialogue, item_gain, quest_update로 이어진다.\n";
    out << "- 전진 행동은 현재 장면의 위험이 분명할 때 combat으로 이어질 수 있다.\n";
    out << "- 이미 전투 중인 장면이라면 적을 제압, 탈출, 협상했다는 근거가 없을 때 combat을 유지한다.\n";
    out << "- 같은 유형의 이벤트를 반복하더라도 최근 사건과 현재 목표에 맞는 새 정보나 변화를 준다.\n";
    out << "- location은 이번 장면의 현재 지역명이다. 이동/진행이 일어나면 새 지역명으로 바꾸고, 같은 장소면 현재 위치를 유지한다.\n";
    out << "- combat을 선택할 때만 monster 객체를 채우고, 다른 event_type에서는 monster를 null로 둔다.\n";
    out << "- item_gain을 선택할 때는 item 객체를 채우고, 다른 event_type에서는 꼭 필요한 경우가 아니면 item을 null로 둔다.\n\n";
    out << baseContext(state) << '\n';
    out << "[직전 행동 컨텍스트]\n" << actionContext << "\n\n";
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"scene_text\": \"string\",\n";
    out << "  \"location\": \"string\",\n";
    out << "  \"event_type\": \"story | combat | item_gain | stat_change | dialogue | quest_update | rest | game_end\",\n";
    out << "  \"next_objective\": \"string\",\n";
    out << "  \"decision_hint\": \"string\",\n";
    out << "  \"choices\": [\"string\"],\n";
    out << "  \"monster\": null,\n";
    out << "  \"item\": null,\n";
    out << "  \"stat_changes\": {\"hp\": 0, \"gold\": 0, \"exp\": 0},\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    return out.str();
}

std::string buildActionResultPrompt(const GameState& state, const std::string& customInput, DiceOutcome diceOutcome)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 고유 선택지 결과 판정기다.\n";
    out << "content_type은 negotiation이다. 플레이어의 자유 행동을 d6 결과에 맞게 해석한다.\n";
    out << "d6 결과는 이미 C++ 코드가 정했으니 반드시 따른다.\n";
    out << "실패면 보상보다 손해나 상황 악화를 중심으로 쓴다.\n";
    out << "성공이면 적절한 이득이나 진행을 준다.\n";
    out << "초대박이면 아이템, 큰 보상, 결정적 단서 중 하나를 줄 수 있다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << baseContext(state) << '\n';
    out << "[고유 선택지]\n" << customInput << "\n\n";
    out << "[d6 결과]\n" << diceOutcomeToKorean(diceOutcome) << " (" << diceOutcomeToString(diceOutcome) << ")\n\n";
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"result_text\": \"string\",\n";
    out << "  \"result_type\": \"damage | heal | gold | exp | item | clue | nothing\",\n";
    out << "  \"item_name\": \"string\",\n";
    out << "  \"item_description\": \"string\",\n";
    out << "  \"hp_delta\": 0,\n";
    out << "  \"gold_delta\": 0,\n";
    out << "  \"exp_delta\": 0\n";
    out << "}\n";
    return out.str();
}

std::string buildInitialWorldPrompt()
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 시작 상황 생성기다.\n";
    out << "게임의 첫 위치, 첫 장면, 첫 목표, 첫 판단 기준을 만든다.\n";
    out << "매번 같은 숲, 순찰병, 늑대 구성을 반복하지 않는다.\n";
    out << "플레이어를 즉사시키거나 현대 기술, 총기, SF 요소를 넣지 않는다.\n";
    out << "한국어 JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"location\": \"string\",\n";
    out << "  \"scene_text\": \"string\",\n";
    out << "  \"current_objective\": \"string\",\n";
    out << "  \"decision_hint\": \"string\",\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
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

GameEvent fallbackEvent(const std::string& reason, EventType eventType)
{
    GameEvent event;
    event.eventType = eventType;
    if (eventType == EventType::Combat) {
        event.sceneText = "어둠 속에서 적의 기척이 가까워진다. 당신은 무기를 고쳐 쥐고 전투 태세를 갖춘다.";
        event.nextObjective = "눈앞의 적을 쓰러뜨리고 다음 길을 확보한다";
        event.decisionHint = "기본 공격으로 빠르게 제압할지, 고유 행동으로 전투 상황을 바꿀지 판단해야 한다";
        event.monster = Monster {"그림자 약탈자", "LLM 응답 오류 중에도 전투 흐름을 유지하기 위해 등장한 적.", 10, 1, 0};
        event.memoryNote = "LLM 응답 오류로 안전한 기본 전투 이벤트가 생성되었다.";
    } else {
        event.sceneText = "잠시 주변이 조용해졌다. 당신은 숨을 고르며 다음 행동을 정리한다.";
        event.eventType = EventType::Story;
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

ActionResult fallbackActionResult(const std::string& reason, DiceOutcome diceOutcome)
{
    ActionResult result;
    result.resultText = "고유 행동은 뚜렷한 결과를 만들지 못했다.";
    result.resultType = "nothing";
    if (diceOutcome == DiceOutcome::Failure) {
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

std::string callOllama(const LLMOptions& options, const std::string& prompt)
{
    Ollama client(options.endpoint);
    client.setConnectionTimeout(options.connectionTimeoutSeconds);
    client.setReadTimeout(options.readTimeoutSeconds);
    client.setWriteTimeout(options.readTimeoutSeconds);

    ollama::request request(ollama::message_type::chat);
    request["model"] = options.model;
    request["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", prompt}}
    });
    request["stream"] = false;
    request["think"] = options.think;
    request["format"] = "json";
    request["keep_alive"] = "5m";
    request["options"] = {{"temperature", options.temperature}};

    const ollama::response response = client.chat(request);
    return response.as_simple_string();
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
    if (choices.size() > 5) {
        choices.resize(5);
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
    if (event.eventType != EventType::Combat
        && event.eventType != EventType::GameEnd
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

void repairActionResult(ActionResult& result, DiceOutcome diceOutcome)
{
    if (result.resultText.empty()) {
        result.resultText = "고유 행동은 짧은 여파를 남겼다.";
        result.notes.push_back("result_text was empty");
    }
    if (result.resultType.empty()) {
        result.resultType = "nothing";
        result.notes.push_back("result_type was empty");
    }

    if (diceOutcome == DiceOutcome::Failure) {
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

void repairInitialWorld(InitialWorld& world)
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

} // namespace

LLM::LLM(LLMOptions options)
    : options_(std::move(options))
{
}

GameEvent LLM::generateEvent(const GameState& state, const std::string& playerInput) const
{
    return generateNextEvent(state, playerInput);
}

GameEvent LLM::generateNextEvent(const GameState& state, const std::string& actionContext) const
{
    try {
        return parseEvent(callOllama(options_, buildNextEventPrompt(state, actionContext)), EventType::Invalid);
    } catch (const std::exception& ex) {
        return fallbackEvent(ex.what(), EventType::Story);
    }
}

GameEvent LLM::generateCombatEvent(const GameState& state, const std::string& actionContext) const
{
    try {
        return parseEvent(callOllama(options_, buildCombatPrompt(state, actionContext)), EventType::Combat);
    } catch (const std::exception& ex) {
        return fallbackEvent(ex.what(), EventType::Combat);
    }
}

GameEvent LLM::generateStoryEvent(const GameState& state, const std::string& actionContext) const
{
    try {
        return parseEvent(callOllama(options_, buildStoryPrompt(state, actionContext)), EventType::Story);
    } catch (const std::exception& ex) {
        return fallbackEvent(ex.what(), EventType::Story);
    }
}

ActionResult LLM::generateActionResult(
    const GameState& state,
    const std::string& customInput,
    DiceOutcome diceOutcome) const
{
    try {
        return parseActionResult(callOllama(options_, buildActionResultPrompt(state, customInput, diceOutcome)), diceOutcome);
    } catch (const std::exception& ex) {
        return fallbackActionResult(ex.what(), diceOutcome);
    }
}

InitialWorld LLM::generateInitialWorld() const
{
    try {
        return parseInitialWorld(callOllama(options_, buildInitialWorldPrompt()));
    } catch (const std::exception& ex) {
        return fallbackInitialWorld(ex.what());
    }
}

GameEvent LLM::parseEvent(const std::string& rawText)
{
    return parseEvent(rawText, EventType::Story);
}

GameEvent LLM::parseEvent(const std::string& rawText, EventType fallbackType)
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
        event.eventType = eventTypeFromString(root.value("event_type", "story"));
        const bool restrictToFallbackType = fallbackType == EventType::Combat || fallbackType == EventType::Story;
        if (event.eventType == EventType::Invalid) {
            event.eventType = restrictToFallbackType ? fallbackType : EventType::Story;
            event.validationNotes.push_back("event_type was invalid");
        } else if (restrictToFallbackType && event.eventType != fallbackType) {
            event.eventType = fallbackType;
            event.validationNotes.push_back("event_type was repaired to requested content type");
        }
        event.nextObjective = root.value("next_objective", "");
        event.decisionHint = root.value("decision_hint", "");
        event.choices = readChoices(root, event.eventType);
        event.monster = readMonster(root, event.eventType);
        event.item = readItem(root);

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
        return fallbackEvent(ex.what(), fallbackType);
    }
}

ActionResult LLM::parseActionResult(const std::string& rawText, DiceOutcome diceOutcome)
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
        repairActionResult(result, diceOutcome);
        return result;
    } catch (const std::exception& ex) {
        return fallbackActionResult(ex.what(), diceOutcome);
    }
}

InitialWorld LLM::parseInitialWorld(const std::string& rawText)
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
        repairInitialWorld(world);
        return world;
    } catch (const std::exception& ex) {
        return fallbackInitialWorld(ex.what());
    }
}

} // namespace textrpg::llm
