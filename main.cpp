#include "combat/CombatSystem.hpp"
#include "llm/LLM.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace textrpg::llm;

namespace {

struct PlayerAction {
    bool isCustom = false;
    std::string customText;
    std::string actionContext;
    std::optional<CombatChoice> combatChoice;
    std::optional<StoryChoice> storyChoice;
};

std::string readEnv(const char* name)
{
    if (const char* value = std::getenv(name)) {
        return value;
    }
    return {};
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool tryParseProvider(const std::string& value, LLMProvider& provider)
{
    const auto normalized = toLower(value);
    if (normalized == "openai") {
        provider = LLMProvider::OpenAI;
        return true;
    }
    if (normalized == "ollama") {
        provider = LLMProvider::Ollama;
        return true;
    }
    return false;
}

std::string providerName(LLMProvider provider)
{
    return provider == LLMProvider::Ollama ? "ollama" : "openai";
}

std::string defaultModel(LLMProvider provider)
{
    if (provider == LLMProvider::Ollama) {
        return "0xIbra/supergemma4-26b-uncensored-gguf-v2:Q4_K_M";
    }
    return "gpt-4.1-mini";
}

std::string defaultEndpoint(LLMProvider provider)
{
    return provider == LLMProvider::Ollama ? "http://localhost:11434" : "https://api.openai.com/v1/";
}

LLMOptions makeLlmOptions(int argc, char** argv, int& scriptArgIndex)
{
    LLMOptions options;

    LLMProvider envProvider;
    if (tryParseProvider(readEnv("TEXTRPG_LLM_PROVIDER"), envProvider)) {
        options.provider = envProvider;
    }

    if (options.provider == LLMProvider::OpenAI) {
        options.apiKey = readEnv("OPENAI_API_KEY");
        options.organization = readEnv("OPENAI_ORG");
        options.endpoint = readEnv("OPENAI_API_BASE");
        options.model = readEnv("OPENAI_MODEL");
    } else {
        options.endpoint = readEnv("OLLAMA_ENDPOINT");
        options.model = readEnv("OLLAMA_MODEL");
    }

    scriptArgIndex = 2;
    if (argc >= 2) {
        LLMProvider cliProvider;
        if (tryParseProvider(argv[1], cliProvider)) {
            options.provider = cliProvider;
            scriptArgIndex = 3;
            if (argc >= 3) {
                options.model = argv[2];
            }
        } else {
            options.model = argv[1];
        }
    }

    if (options.endpoint.empty()) {
        options.endpoint = defaultEndpoint(options.provider);
    }
    if (options.model.empty()) {
        options.model = defaultModel(options.provider);
    }

    return options;
}

GameState makeInitialState()
{
    GameState state;
    state.turnNumber = 1;
    state.currentScene = "낯선 길 위에서 첫 여정이 시작된다.";
    state.player.hp = 999;
    state.player.maxHp = 999;
    state.player.level = 1;
    state.player.attack = 9;
    state.player.defense = 0;
    state.player.gold = 12;
    state.player.exp = 0;
    state.player.inventory = {"낡은 철검", "작은 빵", "횃불"};
    state.world.location = "이름 없는 길";
    state.world.currentObjective = "현재 장소의 단서를 확인한다";
    state.world.decisionHint = "정보를 모을지, 빠르게 이동할지 판단해야 한다";
    state.world.fixedRules = {
        "배경은 중세 판타지 세계다.",
        "현대 기술과 총기는 등장하지 않는다.",
        "플레이어를 즉사시키지 않는다.",
        "전투 계산과 보상 확정은 C++ 엔진이 담당한다.",
    };
    return state;
}

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

void printStatus(const GameState& state, EventRoll sceneType)
{
    std::cout << "\n============================================================\n";
    std::cout << "턴 " << state.turnNumber << " | " << state.world.location
              << " | " << (sceneType == EventRoll::Combat ? "전투" : "비전투") << '\n';
    std::cout << "HP " << state.player.hp << "/" << state.player.maxHp
              << " | LV " << state.player.level
              << " | Gold " << state.player.gold
              << " | Exp " << state.player.exp << '\n';
    std::cout << "목표: " << state.world.currentObjective << '\n';
    std::cout << "판단 기준: " << state.world.decisionHint << '\n';
    std::cout << "============================================================\n";
}

void printEvent(const GameEvent& event)
{
    std::cout << "\n[" << eventTypeToString(event.eventType) << "]";
    if (event.usedFallback) {
        std::cout << " fallback";
    }
    std::cout << "\n\n" << event.sceneText << "\n";

    if (!event.location.empty()) {
        std::cout << "\n현재 지역: " << event.location << '\n';
    }

    if (!event.nextObjective.empty()) {
        std::cout << "\n다음 목표: " << event.nextObjective << '\n';
    }
    if (!event.decisionHint.empty()) {
        std::cout << "판단 기준: " << event.decisionHint << '\n';
    }

    if (event.monster.has_value()) {
        const auto& monster = event.monster.value();
        std::cout << "\n몬스터: " << monster.name
                  << " (HP " << monster.hp
                  << ", ATK " << monster.attack
                  << ", DEF " << monster.defense << ")\n";
        if (!monster.description.empty()) {
            std::cout << monster.description << '\n';
        }
    }

    if (event.item.has_value()) {
        const auto& item = event.item.value();
        std::cout << "\n획득 후보: " << item.name
                  << " [" << itemTypeToString(item.type) << "]"
                  << " value=" << item.value << '\n';
        if (!item.description.empty()) {
            std::cout << item.description << '\n';
        }
    }

    std::cout << "\n상태 변화: HP " << event.statChanges.hp
              << ", Gold " << event.statChanges.gold
              << ", Exp " << event.statChanges.exp << '\n';

    if (!event.validationNotes.empty()) {
        std::cout << "\n시스템 노트:\n";
        for (const auto& note : event.validationNotes) {
            std::cout << "  - " << note << '\n';
        }
    }
}

void printCombatResult(const textrpg::combat::CombatResult& result)
{
    std::cout << "\n전투 결과:\n";
    for (const auto& turn : result.turns) {
        const auto actor = turn.actor == textrpg::combat::CombatActor::Player ? "플레이어" : result.monster.name;
        const auto target = turn.actor == textrpg::combat::CombatActor::Player ? result.monster.name : "플레이어";
        std::cout << "  " << actor << "의 때리기 -> " << target
                  << "에게 " << turn.damage << " 피해"
                  << " (남은 HP " << turn.targetHpAfter << ")\n";
    }

    if (result.winner == textrpg::combat::CombatWinner::Player) {
        std::cout << "  승리: 플레이어\n";
    } else {
        std::cout << "  패배: 플레이어\n";
    }
}

void printChoiceMenu(EventRoll sceneType)
{
    std::cout << "\n선택지:\n";
    if (sceneType == EventRoll::Combat) {
        std::cout << "  1. 공격\n";
        std::cout << "  2. 스킬 (준비 중)\n";
        std::cout << "  3. 아이템 (준비 중)\n";
        std::cout << "  4. 고유 행동 입력\n";
    } else {
        std::cout << "  1. 전진\n";
        std::cout << "  2. 조사\n";
        std::cout << "  3. 고유 행동 입력\n";
    }
    std::cout << "번호를 고르거나 직접 행동을 입력하세요. (종료: q)\n";
}

bool isExitCommand(const std::string& input)
{
    return input == "q" || input == "quit" || input == "exit" || input == "종료";
}

bool isUnsignedInteger(const std::string& input)
{
    return !input.empty()
        && std::all_of(input.begin(), input.end(), [](unsigned char ch) {
               return std::isdigit(ch) != 0;
           });
}

std::vector<std::string> splitScriptedInputs(const std::string& script)
{
    std::vector<std::string> inputs;
    std::size_t start = 0;
    while (start <= script.size()) {
        const auto delimiter = script.find('|', start);
        const auto end = delimiter == std::string::npos ? script.size() : delimiter;
        const auto input = script.substr(start, end - start);
        if (!input.empty()) {
            inputs.push_back(input);
        }
        if (delimiter == std::string::npos) {
            break;
        }
        start = delimiter + 1;
    }
    return inputs;
}

std::string readPlayerInput(EventRoll sceneType)
{
    printChoiceMenu(sceneType);
    std::cout << "\n행동 입력: ";

    std::string input;
    std::getline(std::cin, input);
    return input;
}

PlayerAction parsePlayerAction(EventRoll sceneType, std::string input)
{
    if (input.empty()) {
        input = sceneType == EventRoll::Combat ? "1" : "1";
    }

    PlayerAction action;
    if (isUnsignedInteger(input)) {
        const auto index = std::stoul(input);
        if (sceneType == EventRoll::Combat) {
            if (index == 1) {
                action.combatChoice = CombatChoice::Attack;
                action.actionContext = "플레이어는 기본 전투 선택지 [공격]을 선택했다.";
                return action;
            }
            if (index == 2) {
                action.combatChoice = CombatChoice::Skill;
                action.actionContext = "플레이어는 [스킬]을 고르려 했지만 아직 사용할 스킬이 없어 자세를 가다듬었다.";
                return action;
            }
            if (index == 3) {
                action.combatChoice = CombatChoice::Item;
                action.actionContext = "플레이어는 [아이템]을 확인했지만 지금 사용할 물건은 정하지 못했다.";
                return action;
            }
            if (index == 4) {
                action.combatChoice = CombatChoice::Custom;
                action.isCustom = true;
                action.customText = "전투 상황을 뒤집을 고유 행동을 시도한다";
                return action;
            }
        } else {
            if (index == 1) {
                action.storyChoice = StoryChoice::Advance;
                action.actionContext = "플레이어는 기본 스토리 선택지 [전진]을 선택했다.";
                return action;
            }
            if (index == 2) {
                action.storyChoice = StoryChoice::Investigate;
                action.actionContext = "플레이어는 기본 스토리 선택지 [조사]를 선택했다.";
                return action;
            }
            if (index == 3) {
                action.isCustom = true;
                action.customText = "상황을 바꿀 고유 행동을 시도한다";
                return action;
            }
        }
    }

    action.isCustom = true;
    action.customText = input;
    return action;
}

std::pair<int, std::string> rollD6()
{
    const int value = (std::rand() % 6) + 1;
    if (value <= 2) {
        return {value, ids::dice::Failure};
    }
    if (value <= 4) {
        return {value, ids::dice::Success};
    }
    return {value, ids::dice::Jackpot};
}

std::string actionResultSummary(const ActionResult& result)
{
    std::ostringstream out;
    out << result.resultText;
    out << " (type=" << result.resultType
        << ", HP " << result.hpDelta
        << ", Gold " << result.goldDelta
        << ", Exp " << result.expDelta;
    if (!result.itemName.empty()) {
        out << ", Item " << result.itemName;
    }
    out << ")";
    return out.str();
}

void printActionResult(const ActionResult& result, int diceValue, const std::string& outcome)
{
    std::cout << "\n고유 행동 d6: " << diceValue << " -> " << diceOutcomeToKorean(outcome) << '\n';
    std::cout << result.resultText << '\n';
    std::cout << "행동 결과: HP " << result.hpDelta
              << ", Gold " << result.goldDelta
              << ", Exp " << result.expDelta << '\n';
    if (!result.itemName.empty()) {
        std::cout << "획득 아이템: " << result.itemName << '\n';
        if (!result.itemDescription.empty()) {
            std::cout << result.itemDescription << '\n';
        }
    }
    if (!result.notes.empty()) {
        std::cout << "시스템 노트:\n";
        for (const auto& note : result.notes) {
            std::cout << "  - " << note << '\n';
        }
    }
}

std::string resolveAttack(GameState& state, std::optional<Monster>& activeMonster)
{
    if (!activeMonster.has_value()) {
        return "플레이어는 공격 자세를 취했지만 눈앞에 확실한 적은 없었다.";
    }

    textrpg::combat::CombatSystem combatSystem;
    auto player = textrpg::combat::makeDefaultPlayer();
    player.hp = state.player.hp;
    auto monster = textrpg::combat::makeDefaultMonster(activeMonster->name);

    const auto result = combatSystem.run(player, monster);
    printCombatResult(result);

    state.player.hp = result.player.hp;
    const auto summary = "플레이어가 기본 선택지 [공격]으로 " + result.monster.name + "를 쓰러뜨렸다.";
    pushLimited(state.memory.recentEvents, "전투 결과: " + summary, 5);
    activeMonster.reset();
    return summary;
}

std::string resolveDefaultAction(GameState& state, std::optional<Monster>& activeMonster, const PlayerAction& action)
{
    if (action.combatChoice.has_value()) {
        if (*action.combatChoice == CombatChoice::Attack) {
            return resolveAttack(state, activeMonster);
        }
        return action.actionContext;
    }

    return action.actionContext;
}

} // namespace

int main(int argc, char** argv)
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    int scriptArgIndex = 2;
    LLMOptions llmOptions = makeLlmOptions(argc, argv, scriptArgIndex);
    std::vector<std::string> scriptedInputs = argc > scriptArgIndex
        ? splitScriptedInputs(argv[scriptArgIndex])
        : std::vector<std::string> {};
    std::size_t scriptedIndex = 0;

    LLM llm(llmOptions);

    GameState state = makeInitialState();
    const auto initialWorld = llm.generateInitialWorld(state);
    EventRoll currentSceneType = EventRoll::NonCombat;
    std::optional<Monster> activeMonster;

    std::cout << "LLM Text RPG demo 시작\n";
    std::cout << "provider: " << providerName(llmOptions.provider) << "\n";
    std::cout << "model: " << llmOptions.model << "\n";
    if (initialWorld.usedFallback) {
        std::cout << "시작 상황 fallback 사용\n";
    }
    std::cout << "다음 장면 유형은 LLM이 고릅니다. 고유 행동은 d6 판정을 사용합니다.\n";

    while (state.player.hp > 0) {
        printStatus(state, currentSceneType);

        std::string input;
        if (scriptedIndex < scriptedInputs.size()) {
            input = scriptedInputs[scriptedIndex++];
            printChoiceMenu(currentSceneType);
            std::cout << "\n행동 입력: " << input << '\n';
        } else {
            input = readPlayerInput(currentSceneType);
        }

        if (isExitCommand(input)) {
            std::cout << "게임을 종료합니다.\n";
            return 0;
        }

        auto playerAction = parsePlayerAction(currentSceneType, input);
        std::string actionContext;

        if (playerAction.isCustom) {
            const auto [diceValue, outcome] = rollD6();
            const auto actionResult = llm.generateActionResult(state, playerAction.customText, outcome);
            printActionResult(actionResult, diceValue, outcome);

            std::ostringstream out;
            out << "고유 행동: " << playerAction.customText << '\n';
            out << "d6: " << diceValue << " -> " << diceOutcomeToKorean(outcome) << '\n';
            out << "결과: " << actionResultSummary(actionResult);
            actionContext = out.str();
        } else {
            actionContext = resolveDefaultAction(state, activeMonster, playerAction);
        }

        const auto event = llm.generateNextEvent(state, actionContext);
        std::cout << "\n다음 이벤트: LLM 선택 -> " << eventTypeToString(event.eventType) << '\n';

        printEvent(event);

        if (event.eventType == ids::event::Combat && event.monster.has_value()) {
            currentSceneType = EventRoll::Combat;
            activeMonster = event.monster;
        } else {
            currentSceneType = EventRoll::NonCombat;
            activeMonster.reset();
        }

        if (event.eventType == ids::event::GameEnd) {
            std::cout << "\n게임 종료 이벤트가 발생했습니다.\n";
            return 0;
        }

        if (argc > scriptArgIndex && scriptedIndex >= scriptedInputs.size()) {
            std::cout << "\n스크립트 입력 실행을 마쳤습니다.\n";
            return event.usedFallback ? 2 : 0;
        }

        std::cout << "\n계속하려면 Enter를 누르세요.";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::cout << "플레이어 HP가 0이 되어 게임을 종료합니다.\n";
    return 0;
}
