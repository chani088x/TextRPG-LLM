#include "llm/ResponseParser.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <sstream>
#include <string>

namespace textrpg::llm::internals {
namespace {

std::string trimAscii(std::string value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

std::string limitNonEmptyLines(const std::string& value, std::size_t maxLines)
{
    std::istringstream input(value);
    std::ostringstream out;
    std::string line;
    std::size_t count = 0;
    while (count < maxLines && std::getline(input, line)) {
        line = trimAscii(line);
        if (line.empty()) {
            continue;
        }
        if (count != 0) {
            out << '\n';
        }
        out << line;
        ++count;
    }
    return out.str();
}

std::string limitSentences(const std::string& value, std::size_t maxSentences)
{
    std::string out;
    std::size_t count = 0;
    for (const char ch : value) {
        out.push_back(ch);
        if (ch == '.' || ch == '!' || ch == '?') {
            ++count;
            if (count >= maxSentences) {
                break;
            }
        }
    }
    return trimAscii(out);
}

std::size_t nextUtf8Index(const std::string& value, std::size_t index)
{
    const auto ch = static_cast<unsigned char>(value[index]);
    if ((ch & 0x80) == 0) {
        return index + 1;
    }
    if ((ch & 0xE0) == 0xC0) {
        return std::min(index + 2, value.size());
    }
    if ((ch & 0xF0) == 0xE0) {
        return std::min(index + 3, value.size());
    }
    if ((ch & 0xF8) == 0xF0) {
        return std::min(index + 4, value.size());
    }
    return index + 1;
}

std::string limitCodepoints(const std::string& value, std::size_t maxCodepoints)
{
    std::size_t index = 0;
    std::size_t count = 0;
    while (index < value.size() && count < maxCodepoints) {
        index = nextUtf8Index(value, index);
        ++count;
    }
    if (index >= value.size()) {
        return value;
    }
    return trimAscii(value.substr(0, index)) + "...";
}

std::string compactText(const std::string& value, std::size_t maxLines, std::size_t maxSentences, std::size_t maxCodepoints)
{
    auto compacted = limitNonEmptyLines(value, maxLines);
    compacted = limitSentences(compacted, maxSentences);
    compacted = limitCodepoints(compacted, maxCodepoints);
    return trimAscii(compacted);
}

class ResponseRepairer {
public:
    static void repairPrologue(Prologue& prologue)
    {
        if (prologue.text.empty()) {
            prologue.text =
                "나는 비에 젖은 국경 마을의 처마 아래에서 오래 접어 둔 이름을 다시 떠올렸다.\n"
                "그 이름의 주인은 사라졌고, 나는 그날 아무것도 하지 못했다는 사실을 아직도 몸 안에 품고 있다. "
                "주머니에는 대단한 무기도, 믿을 만한 증표도 없었다. 남은 것은 몇 번이고 되짚은 소문과, 마지막 목격자가 이 마을에 있다는 희미한 말뿐이었다.\n"
                "밤비는 골목의 흙냄새를 끌어올렸고, 닫힌 창문 뒤에서는 사람들이 숨을 죽인 채 나그네의 발소리를 들었다. "
                "나는 먼저 마을에 남은 마지막 목격담을 확인하고, 그 말이 가리키는 길을 따라 사라진 사람의 진실에 닿기로 했다.";
            prologue.validationNotes.push_back("text was empty");
        }
        if (prologue.protagonistWound.empty()) {
            prologue.protagonistWound = "잃어버린 사람의 흔적";
            prologue.validationNotes.push_back("protagonist_wound was empty");
        }
        if (prologue.personalGoal.empty()) {
            prologue.personalGoal = "사라진 이의 행방을 찾는다";
            prologue.validationNotes.push_back("personal_goal was empty");
        }
        if (prologue.openingLocation.empty()) {
            prologue.openingLocation = "비에 젖은 국경 마을";
            prologue.validationNotes.push_back("opening_location was empty");
        }
        if (prologue.firstObjective.empty()) {
            prologue.firstObjective = "마을에 남은 마지막 목격담을 확인한다";
            prologue.validationNotes.push_back("first_clue was empty");
        }
        if (prologue.memoryNote.empty()) {
            prologue.memoryNote = "주인공은 잃어버린 사람의 흔적을 찾기 위해 여정을 시작했다.";
            prologue.validationNotes.push_back("memory_note was empty");
        }
        prologue.generated = true;
    }

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
        if (event.eventType == ids::event::Combat || event.eventType == ids::event::GameEnd) {
            event.baseCandidate = false;
        }
        if (event.eventType != ids::event::Combat
            && event.eventType != ids::event::GameEnd
            && event.choices.size() < 2) {
            event.choices = {
                "눈앞의 단서를 살핀다",
                "목표 방향으로 움직인다",
            };
            event.validationNotes.push_back("choices were repaired");
        }

        event.sceneText = compactText(event.sceneText, 4, 4, 260);
        event.nextObjective = compactText(event.nextObjective, 1, 1, 90);
        event.decisionHint = compactText(event.decisionHint, 1, 1, 90);
        event.memoryNote = compactText(event.memoryNote, 1, 1, 100);
        event.statChanges.hp = clampInt(event.statChanges.hp, -30, 30);
        event.statChanges.gold = clampInt(event.statChanges.gold, 0, 100);
        event.statChanges.exp = clampInt(event.statChanges.exp, 0, 50);
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
        world.sceneText = compactText(world.sceneText, 4, 4, 260);
        world.currentObjective = compactText(world.currentObjective, 1, 1, 90);
        world.decisionHint = compactText(world.decisionHint, 1, 1, 90);
        world.memoryNote = compactText(world.memoryNote, 1, 1, 100);
    }

    static void repairActionResult(ActionResult& result)
    {
        if (result.resultText.empty()) {
            result.resultText = "행동의 결과가 뚜렷하게 드러나지 않았다. 당신은 잠시 숨을 고르며 다음 상황을 살핀다.";
            result.validationNotes.push_back("result_text was empty");
        }
        if (result.memoryNote.empty()) {
            result.memoryNote = "고유 행동의 결과가 정리되었다.";
            result.validationNotes.push_back("memory_note was empty");
        }
        if (result.nextEventHint.empty()) {
            result.nextEventHint = "직전 행동 결과에 자연스럽게 이어지는 다음 장면을 선택한다";
            result.validationNotes.push_back("next_event_hint was empty");
        }

        result.resultText = compactText(result.resultText, 3, 3, 220);
        result.nextEventHint = compactText(result.nextEventHint, 1, 1, 90);
        result.memoryNote = compactText(result.memoryNote, 1, 1, 100);
        result.statChanges.hp = clampInt(result.statChanges.hp, -30, 30);
        result.statChanges.gold = clampInt(result.statChanges.gold, 0, 100);
        result.statChanges.exp = clampInt(result.statChanges.exp, 0, 50);
    }

    static void repairElderDialogue(ElderDialogueResult& result)
    {
        if (result.dialogue.empty()) {
            result.dialogue = "장로: 오래 기다렸네. 이제 네가 알아야 할 이름이 있다.";
            result.validationNotes.push_back("dialogue was empty");
        }
        if (result.boss.name.empty()) {
            result.boss.name = "이름 없는 최종 위협";
            result.validationNotes.push_back("boss.name was empty");
        }
        if (result.boss.location.empty()) {
            result.boss.location = "북쪽 폐허의 지하 제단";
            result.validationNotes.push_back("boss.location was empty");
        }
        if (result.boss.weakness.empty()) {
            result.boss.weakness = "이미 얻은 단서와 성자의 흔적";
            result.validationNotes.push_back("boss.weakness was empty");
        }
        if (result.boss.description.empty()) {
            result.boss.description = "이 지역의 불길한 사건들을 뒤에서 움직이는 최종 위협.";
            result.validationNotes.push_back("boss.description was empty");
        }
        if (result.questUpdate.empty()) {
            result.questUpdate = "장로에게서 보스의 정체와 목적지를 들었다.";
            result.validationNotes.push_back("quest_update was empty");
        }
        if (result.memoryNote.empty()) {
            result.memoryNote = "장로 대화로 보스 정보가 확정되었다.";
            result.validationNotes.push_back("memory_note was empty");
        }
        result.dialogue = compactText(result.dialogue, 6, 6, 360);
        result.boss.name = compactText(result.boss.name, 1, 1, 40);
        result.boss.location = compactText(result.boss.location, 1, 1, 50);
        result.boss.weakness = compactText(result.boss.weakness, 1, 1, 60);
        result.boss.description = compactText(result.boss.description, 2, 2, 140);
        result.questUpdate = compactText(result.questUpdate, 1, 1, 120);
        result.memoryNote = compactText(result.memoryNote, 1, 1, 100);
        result.boss.known = true;
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
            choices.push_back(compactText(choice.get<std::string>(), 1, 1, 32));
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
        clampInt(monster.value("hp", 10), 1, 60),
        clampInt(monster.value("attack", 1), 1, 12),
        clampInt(monster.value("defense", 0), 0, 8),
    };
}

std::optional<Item> readItem(const nlohmann::json& root, std::vector<std::string>& notes)
{
    if (!root.contains("item") || !root["item"].is_object()) {
        return std::nullopt;
    }

    const auto& item = root["item"];
    Item parsed {
        item.value("name", std::string("알 수 없는 물건")),
        item.value("type", std::string(ids::item::Consumable)),
        item.value("description", std::string("")),
        item.value("value", 0),
    };
    sanitizeItem(parsed, &notes);
    return parsed;
}

BossInfo readBossInfo(const nlohmann::json& root)
{
    BossInfo boss;
    if (!root.contains("boss") || !root["boss"].is_object()) {
        return boss;
    }

    const auto& value = root["boss"];
    boss.name = value.value("name", std::string {});
    boss.location = value.value("location", std::string {});
    boss.weakness = value.value("weakness", std::string {});
    boss.description = value.value("description", std::string {});
    boss.known = !boss.name.empty() || !boss.location.empty();
    return boss;
}

} // namespace

Prologue ResponseParser::fallbackPrologue(const std::string& reason)
{
    Prologue prologue;
    prologue.generated = true;
    prologue.text =
        "나는 비에 젖은 국경 마을의 처마 아래에서 오래 접어 둔 이름을 다시 떠올렸다.\n"
        "그 이름의 주인은 사라졌고, 나는 그날 아무것도 하지 못했다는 사실을 아직도 몸 안에 품고 있다. "
        "주머니에는 대단한 무기도, 믿을 만한 증표도 없었다. 남은 것은 몇 번이고 되짚은 소문과, 마지막 목격자가 이 마을에 있다는 희미한 말뿐이었다.\n"
        "밤비는 골목의 흙냄새를 끌어올렸고, 닫힌 창문 뒤에서는 사람들이 숨을 죽인 채 나그네의 발소리를 들었다. "
        "나는 먼저 마을에 남은 마지막 목격담을 확인하고, 그 말이 가리키는 길을 따라 사라진 사람의 진실에 닿기로 했다.";
    prologue.protagonistWound = "사라진 사람을 지키지 못했다는 죄책감";
    prologue.personalGoal = "사라진 사람의 행방을 찾아 진실을 확인한다";
    prologue.openingLocation = "비에 젖은 국경 마을";
    prologue.firstObjective = "마을에 남은 마지막 목격담을 확인한다";
    prologue.memoryNote = "주인공은 사라진 사람의 흔적을 따라 국경 마을에 도착했다.";
    prologue.usedFallback = true;
    prologue.validationNotes.push_back(reason);
    return prologue;
}

GameEvent ResponseParser::fallbackEvent(const std::string& reason, const std::string& eventType)
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
            "흔적이 남은 바닥과 주변 표식을 살핀다",
            "소리를 낮추고 안전한 우회로를 찾아 이동한다",
        };
        event.memoryNote = "LLM 응답 오류로 안전한 기본 이벤트가 생성되었다.";
    }
    event.usedFallback = true;
    event.validationNotes.push_back(reason);
    return event;
}

InitialWorld ResponseParser::fallbackInitialWorld(const std::string& reason)
{
    InitialWorld world;
    world.location = "이름 없는 길";
    world.sceneText = "낯선 길 위로 오래된 표식과 희미한 발자국이 이어진다.";
    world.currentObjective = "낯선 표식의 의미를 확인한다";
    world.decisionHint = "표식을 살피면 단서를 얻지만, 이동하면 더 빨리 다음 장소에 닿을 수 있다";
    world.memoryNote = "LLM 시작 상황 생성 실패로 기본 시작 상황이 생성되었다.";
    world.usedFallback = true;
    world.notes.push_back(reason);
    return world;
}

ActionResult ResponseParser::fallbackActionResult(const std::string& reason)
{
    ActionResult result;
    result.resultText = "행동의 결과가 불안정하게 흐려졌다. 당신은 큰 변화 없이 자세를 가다듬고 주변 상황을 다시 살핀다.";
    result.nextEventHint = "안전한 story 또는 quest_update로 이어간다";
    result.memoryNote = "LLM 응답 오류로 안전한 고유 행동 결과가 생성되었다.";
    result.usedFallback = true;
    result.validationNotes.push_back(reason);
    return result;
}

ElderDialogueResult ResponseParser::fallbackElderDialogue(const std::string& reason)
{
    ElderDialogueResult result;
    result.dialogue =
        "장로: 네가 이 거점을 세웠다면, 이제 진실을 들을 때가 되었네.\n"
        "플레이어: 누구를 찾아야 합니까?\n"
        "장로: 북쪽 폐허의 지하 제단에 숨은 이름 없는 최종 위협이다. 성자의 흔적이 그 약점이 될 걸세.";
    result.boss.known = true;
    result.boss.name = "이름 없는 최종 위협";
    result.boss.location = "북쪽 폐허의 지하 제단";
    result.boss.weakness = "성자의 흔적";
    result.boss.description = "LLM 응답 오류 중에도 메인 진행을 유지하기 위해 확정된 최종 위협.";
    result.questUpdate = "장로에게서 보스의 정체와 위치를 들었다.";
    result.memoryNote = "LLM 응답 오류로 기본 장로 대화와 보스 정보가 확정되었다.";
    result.usedFallback = true;
    result.validationNotes.push_back(reason);
    return result;
}

Prologue ResponseParser::parsePrologue(const std::string& rawText)
{
    std::string jsonText;
    if (!extractJsonObject(rawText, jsonText)) {
        return fallbackPrologue("no JSON object found");
    }

    try {
        const auto root = nlohmann::json::parse(jsonText);
        if (!root.is_object()) {
            return fallbackPrologue("JSON root is not an object");
        }

        Prologue prologue;
        prologue.text = root.value("text", "");
        prologue.protagonistWound = root.value("protagonist_wound", "");
        prologue.personalGoal = root.value("personal_goal", "");
        prologue.openingLocation = root.value("opening_location", "");
        prologue.firstObjective = root.value(
            "first_clue",
            root.value("first_objective", std::string {}));
        prologue.memoryNote = root.value("memory_note", "");
        ResponseRepairer::repairPrologue(prologue);
        return prologue;
    } catch (const std::exception& ex) {
        return fallbackPrologue(ex.what());
    }
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
        event.baseCandidate = root.value("base_candidate", false);
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

ActionResult ResponseParser::parseActionResult(const std::string& rawText)
{
    std::string jsonText;
    if (!extractJsonObject(rawText, jsonText)) {
        return fallbackActionResult("no JSON object found");
    }

    try {
        const auto root = nlohmann::json::parse(jsonText);
        if (!root.is_object()) {
            return fallbackActionResult("JSON root is not an object");
        }

        ActionResult result;
        result.resultText = root.value("result_text", "");
        result.location = root.value("location", "");
        result.item = readItem(root, result.validationNotes);

        if (root.contains("stat_changes") && root["stat_changes"].is_object()) {
            const auto& stats = root["stat_changes"];
            result.statChanges.hp = stats.value("hp", 0);
            result.statChanges.gold = stats.value("gold", 0);
            result.statChanges.exp = stats.value("exp", 0);
        }

        result.nextEventHint = root.value("next_event_hint", "");
        result.baseCandidate = root.value("base_candidate", false);
        result.memoryNote = root.value("memory_note", "");
        ResponseRepairer::repairActionResult(result);
        return result;
    } catch (const std::exception& ex) {
        return fallbackActionResult(ex.what());
    }
}

ElderDialogueResult ResponseParser::parseElderDialogue(const std::string& rawText)
{
    std::string jsonText;
    if (!extractJsonObject(rawText, jsonText)) {
        return fallbackElderDialogue("no JSON object found");
    }

    try {
        const auto root = nlohmann::json::parse(jsonText);
        if (!root.is_object()) {
            return fallbackElderDialogue("JSON root is not an object");
        }

        ElderDialogueResult result;
        result.dialogue = root.value("dialogue", "");
        result.boss = readBossInfo(root);
        result.questUpdate = root.value("quest_update", "");
        result.memoryNote = root.value("memory_note", "");
        ResponseRepairer::repairElderDialogue(result);
        return result;
    } catch (const std::exception& ex) {
        return fallbackElderDialogue(ex.what());
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
        world.baseCandidate = root.value("base_candidate", false);
        world.memoryNote = root.value("memory_note", "");
        ResponseRepairer::repairInitialWorld(world);
        return world;
    } catch (const std::exception& ex) {
        return fallbackInitialWorld(ex.what());
    }
}

} // namespace textrpg::llm::internals
