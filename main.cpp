#include "combat/CombatSystem.hpp"
#include "llm/LLM.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace textrpg::llm;

namespace {

GameState makeInitialState()
{
    GameState state;
    state.turnNumber = 1;
    state.currentScene = "안개 낀 숲길 갈림목에 부서진 순찰병의 표식과 오래된 발자국이 남아 있다.";
    state.player.hp = 999;
    state.player.maxHp = 999;
    state.player.level = 1;
    state.player.attack = 9;
    state.player.defense = 0;
    state.player.gold = 12;
    state.player.exp = 0;
    state.player.inventory = {"낡은 철검", "작은 빵", "횃불"};
    state.world.location = "안개 숲";
    state.world.currentObjective = "숲 안에서 사라진 순찰병의 흔적을 찾는다";
    state.world.decisionHint = "소리를 쫓아 단서를 얻을지, 안전을 확보할지 판단해야 한다";
    state.world.fixedRules = {
        "배경은 중세 판타지 세계다.",
        "현대 기술과 총기는 등장하지 않는다.",
        "플레이어를 즉사시키지 않는다.",
        "전투 계산과 보상 확정은 C++ 엔진이 담당한다.",
    };
    state.memory.recentEvents = {
        "플레이어는 국경 마을에서 실종된 순찰병 이야기를 들었다.",
        "대장장이는 숲 안쪽에서 이상한 종소리와 깨진 등불을 보았다고 말했다.",
    };
    state.memory.importantChoices = {
        "플레이어는 보상을 먼저 요구하지 않고 수색을 돕기로 했다.",
    };
    return state;
}

void printStatus(const GameState& state)
{
    std::cout << "\n============================================================\n";
    std::cout << "턴 " << state.turnNumber << " | " << state.world.location << '\n';
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

    if (!event.choices.empty()) {
        std::cout << "\n선택지:\n";
        for (std::size_t i = 0; i < event.choices.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << event.choices[i] << '\n';
        }
    }

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
        const auto actor = turn.actor == textrpg::combat::CombatActor::Player ? "플레이어" : result.monster.getName();
        const auto target = turn.actor == textrpg::combat::CombatActor::Player ? result.monster.getName() : "플레이어";
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

std::string readPlayerInput(const std::vector<std::string>& previousChoices)
{
    std::cout << "\n행동 입력";
    if (!previousChoices.empty()) {
        std::cout << " 또는 선택지 번호";
    }
    std::cout << " (종료: q): ";

    std::string input;
    std::getline(std::cin, input);

    if (isUnsignedInteger(input)) {
        const auto index = static_cast<std::size_t>(std::stoul(input));
        if (index >= 1 && index <= previousChoices.size()) {
            return previousChoices[index - 1];
        }
        std::cout << "없는 선택지 번호입니다. 자유 입력으로 처리합니다.\n";
    }

    return input;
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

void resolveCombat(GameState& state, const GameEvent& event)
{
    if (event.eventType != EventType::Combat || !event.monster.has_value()) {
        return;
    }

    textrpg::combat::CombatSystem combatSystem;
    auto player = textrpg::combat::makeDefaultPlayer();
    player.setHp(state.player.hp);
    auto monster = textrpg::combat::makeDefaultMonster(event.monster->name);

    const auto result = combatSystem.run(player, monster);
    printCombatResult(result);

    state.player.hp = result.player.getHp();
    pushLimited(
        state.memory.recentEvents,
        "전투 결과: 플레이어가 때리기만 사용해 " + result.monster.getName() + "를 쓰러뜨렸다.",
        5);
}

void applyEventToState(GameState& state, const GameEvent& event, const std::string& playerInput)
{
    state.currentScene = event.sceneText;

    if (!event.nextObjective.empty()) {
        state.world.currentObjective = event.nextObjective;
    }
    if (!event.decisionHint.empty()) {
        state.world.decisionHint = event.decisionHint;
    }

    if (event.eventType != EventType::Combat) {
        state.player.hp = clampInt(state.player.hp + event.statChanges.hp, 0, state.player.maxHp);
    }
    state.player.gold = std::max(0, state.player.gold + event.statChanges.gold);
    state.player.exp = std::max(0, state.player.exp + event.statChanges.exp);

    if (event.item.has_value()) {
        state.player.inventory.push_back(event.item->name);
    }

    pushLimited(state.memory.importantChoices, "턴 " + std::to_string(state.turnNumber) + " 선택: " + playerInput, 5);
    pushLimited(state.memory.recentEvents, "턴 " + std::to_string(state.turnNumber) + " 결과: " + event.memoryNote, 5);

    if (event.eventType == EventType::Combat && event.monster.has_value()) {
        pushLimited(
            state.memory.recentEvents,
            "전투 후속 지침: 다음에 플레이어가 " + event.monster->name
                + "와 맞서거나 공격하면 새 적을 만들지 말고 전투 결과와 그 직후 발견을 묘사한다.",
            5);
    }

    ++state.turnNumber;
}

} // namespace

int main(int argc, char** argv)
{
    LLMOptions llmOptions;
    llmOptions.model = argc >= 2 ? argv[1] : "llama3.2:latest";
    std::vector<std::string> scriptedInputs = argc >= 3 ? splitScriptedInputs(argv[2]) : std::vector<std::string> {};
    std::size_t scriptedIndex = 0;

    LLM llm(llmOptions);

    GameState state = makeInitialState();
    std::vector<std::string> lastChoices;

    std::cout << "LLM Text RPG demo 시작\n";
    std::cout << "model: " << llmOptions.model << "\n";
    std::cout << "전투는 별도 모듈에서 처리합니다. 현재 스킬은 양쪽 모두 때리기만 있습니다.\n";

    while (state.player.hp > 0) {
        printStatus(state);

        std::string playerInput;
        if (scriptedIndex < scriptedInputs.size()) {
            playerInput = scriptedInputs[scriptedIndex++];
            std::cout << "\n행동 입력: " << playerInput << '\n';
        } else {
            playerInput = readPlayerInput(lastChoices);
        }

        if (isExitCommand(playerInput)) {
            std::cout << "게임을 종료합니다.\n";
            return 0;
        }

        if (playerInput.empty()) {
            playerInput = "주변을 살핀다";
        }

        const auto event = llm.generateEvent(state, playerInput);
        printEvent(event);
        resolveCombat(state, event);
        applyEventToState(state, event, playerInput);
        lastChoices = event.choices;

        if (event.eventType == EventType::GameEnd) {
            std::cout << "\n게임 종료 이벤트가 발생했습니다.\n";
            return 0;
        }

        if (argc >= 3 && scriptedIndex >= scriptedInputs.size()) {
            std::cout << "\n스크립트 입력 실행을 마쳤습니다.\n";
            return event.usedFallback ? 2 : 0;
        }

        std::cout << "\n계속하려면 Enter를 누르세요.";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::cout << "플레이어 HP가 0이 되어 게임을 종료합니다.\n";
    return 0;
}
