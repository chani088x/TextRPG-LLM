#include "llm/PromptBuilder.hpp"

#include <sstream>

namespace textrpg::llm::internals {
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
    out << "고정 규칙:\n" << joinLimited(state.world.fixedRules, 8, "없음");
    out << "인벤토리:\n" << joinLimited(state.player.inventory, 8, "없음");
    out << "최근 사건:\n" << joinLimited(state.memory.recentEvents, 5, "없음");
    out << "중요 선택:\n" << joinLimited(state.memory.importantChoices, 5, "없음");
    return out.str();
}

} // namespace

std::string PromptBuilder::buildCombatPrompt(const GameState& state, const std::string& actionContext)
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
    out << "  \"event_type\": \"" << ids::event::Combat << "\",\n";
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

std::string PromptBuilder::buildStoryPrompt(const GameState& state, const std::string& actionContext)
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
    out << "  \"event_type\": \"" << ids::event::Story << "\",\n";
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

std::string PromptBuilder::buildNextEventPrompt(const GameState& state, const std::string& actionContext)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 다음 장면 디렉터다.\n";
    out << "현재 상태, 최근 사건, 플레이어의 직전 행동을 보고 다음 event_type을 직접 고른다.\n";
    out << "전투 여부를 랜덤처럼 고르지 말고, 직전 행동의 결과로 자연스럽게 이어지는 장면을 만든다.\n";
    out << "전투 계산과 보상 확정은 C++ 엔진이 담당하므로 수치 결과를 과장하지 않는다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << "[허용 event_type]\n" << joinIds(eventTypeIds()) << "\n\n";
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

std::string PromptBuilder::buildActionResultPrompt(
    const GameState& state,
    const std::string& customInput,
    const std::string& diceOutcome)
{
    const auto normalizedOutcome = normalizeDiceOutcome(diceOutcome);

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
    out << "[d6 결과]\n" << diceOutcomeToKorean(normalizedOutcome)
        << " (" << normalizedOutcome << ")\n\n";
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

std::string PromptBuilder::buildInitialWorldPrompt()
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

} // namespace textrpg::llm::internals
