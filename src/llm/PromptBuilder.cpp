#include "llm/PromptBuilder.hpp"

#include "llm/RagContext.hpp"

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

std::string joinItems(const std::vector<Item>& items, std::size_t maxItems)
{
    if (items.empty()) {
        return "- 없음\n";
    }

    std::ostringstream out;
    const auto start = items.size() > maxItems ? items.size() - maxItems : 0;
    for (std::size_t i = start; i < items.size(); ++i) {
        out << "- " << items[i].name
            << " [" << items[i].type << "]"
            << " value=" << items[i].value;
        if (!items[i].description.empty()) {
            out << ": " << items[i].description;
        }
        out << '\n';
    }
    return out.str();
}

std::string joinEnemies(const std::vector<Monster>& enemies, std::size_t maxItems)
{
    if (enemies.empty()) {
        return "- 없음\n";
    }

    std::ostringstream out;
    const auto start = enemies.size() > maxItems ? enemies.size() - maxItems : 0;
    for (std::size_t i = start; i < enemies.size(); ++i) {
        out << "- " << enemies[i].name;
        if (!enemies[i].description.empty()) {
            out << ": " << enemies[i].description;
        }
        out << '\n';
    }
    return out.str();
}

std::string joinBaseFeatures(const GameRecords::BaseState& base)
{
    if (!base.unlocked) {
        return "- 잠김\n";
    }

    std::ostringstream out;
    out << "- 거점 위치: " << (base.location.empty() ? "미지정" : base.location) << '\n';
    out << "- 해방 기능:\n";
    out << joinLimited(base.features, 8, "없음");
    return out.str();
}

std::string bossContext(const BossInfo& boss)
{
    if (!boss.known) {
        return "- 미확인\n";
    }

    std::ostringstream out;
    out << "- 이름: " << boss.name << '\n';
    out << "- 위치: " << boss.location << '\n';
    out << "- 약점: " << boss.weakness << '\n';
    if (!boss.description.empty()) {
        out << "- 설명: " << boss.description << '\n';
    }
    return out.str();
}

std::string prologueContext(const Prologue& prologue)
{
    if (!prologue.generated) {
        return "- 미생성\n";
    }

    std::ostringstream out;
    out << "- 개인 동기: " << prologue.personalGoal << '\n';
    out << "- 결핍/상처: " << prologue.protagonistWound << '\n';
    out << "- 시작 위치: " << prologue.openingLocation << '\n';
    out << "- 첫 실마리: " << prologue.firstObjective << '\n';
    out << "- 요약: " << prologue.memoryNote << '\n';
    return out.str();
}

std::string baseContext(const GameState& state)
{
    std::ostringstream out;
    out << "[현재 상태]\n";
    out << "턴: " << state.turnNumber << '\n';
    out << "위치: " << state.world.location << '\n';
    out << "현재 장면: " << state.currentScene << '\n';
    out << ((state.records.elder.talked || state.records.boss.known) ? "현재 목표: " : "현재 실마리: ")
        << state.world.currentObjective << '\n';
    out << "현재 판단 기준: " << state.world.decisionHint << '\n';
    out << "현재 위치 거점 후보: " << (state.world.baseCandidate ? "true" : "false") << '\n';
    out << "플레이어 HP/ATK: " << state.player.hp << "/" << state.player.attack << '\n';
    out << "위험도: " << state.records.danger.level
        << "/" << state.records.danger.threshold
        << " (최근 증가 +" << state.records.danger.lastIncrease << ")\n";
    out << "Gold/Exp: " << state.player.gold << "/" << state.player.exp << '\n';
    out << "고정 규칙:\n" << joinLimited(state.world.fixedRules, 8, "없음");
    out << "프롤로그:\n" << prologueContext(state.records.prologue);
    out << "인벤토리:\n" << joinItems(state.player.inventory, 8);
    out << "거점 상태:\n" << joinBaseFeatures(state.records.base);
    out << "장로 대화: " << (state.records.elder.talked ? "완료" : "미완료") << '\n';
    out << "보스 정보:\n" << bossContext(state.records.boss);
    out << "목표 기록:\n" << joinLimited(state.records.questLog, 8, "장로 대화 전에는 정식 목표 없음");
    out << "획득 아이템 기록:\n" << joinItems(state.records.obtainedItems, 8);
    out << "조우한 적 기록:\n" << joinEnemies(state.records.encounteredEnemies, 8);
    out << "최근 사건:\n" << joinLimited(state.memory.recentEvents, 5, "없음");
    out << "중요 선택:\n" << joinLimited(state.memory.importantChoices, 5, "없음");
    out << "기록 유지 규칙:\n";
    out << "- 이미 기록된 아이템의 이름, 종류, 설명, 가치를 갑자기 바꾸지 않는다.\n";
    out << "- quest_item은 진행용 물건이며 전투 능력이나 초월적 효과를 만들지 않는다.\n";
    out << "- 기록에 있는 소품은 실제로 조사, 사용, 상실, 전달, 해석될 때만 언급한다. 분위기 묘사로 반복하지 않는다.\n";
    out << "- 새 상징 소품을 반복해서 만들지 말고 장소, 인물, 갈등, 행동 결과를 우선 진행한다.\n";
    out << "- d12 초대박은 큰 이점과 함께 실제 획득 가능한 아이템을 줄 수 있다.\n";
    out << "- LLM은 거점 확정이 아니라 현재 위치가 거점 후보인지 base_candidate true/false만 판단한다.\n";
    out << "- base_candidate는 안전한 생활권, 주민/NPC, 장로 존재 가능성, 정비 가능성이 모두 자연스러울 때만 true다.\n";
    out << "- 지하감옥, 폐허, 버려진 성체, 적 본거지, 전투 지역, 위험한 통로, 사람이 살지 않는 장소는 반드시 false다.\n";
    out << "- 애매하면 false로 둔다. C++ 엔진이 true일 때만 플레이어에게 거점 선택을 보여준다.\n";
    out << "- 장로 대화 전에는 정식 목표를 확정하지 말고, 실마리와 상황 변화로 진행한다.\n";
    out << "- 조우한 적과 장로 이후 목표는 위 기록과 모순되지 않게 이어간다.\n";
    return out.str();
}

} // namespace

std::string PromptBuilder::buildProloguePrompt(const GameState& state)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 주인공 프롤로그 생성기다.\n";
    out << "주인공의 입장에서 시작 서사를 만든다. 1인칭 또는 주인공에 매우 가까운 시점으로 쓴다.\n";
    out << "text는 5~8문장, 2~3문단 분량으로 쓴다. 감정, 상처, 현재 위치의 감각, 첫 실마리가 자연스럽게 이어져야 한다.\n";
    out << "주인공은 평범하지만 움직일 이유가 분명한 인물이어야 한다.\n";
    out << "개인적 결핍/상처, 개인 동기, 시작 위치, 첫 실마리를 반드시 만든다.\n";
    out << "시작 장비가 비어 있으면 무기, 방어구, 소모품을 가지고 있다고 쓰지 않는다.\n";
    out << "세계 멸망급 운명, 즉사, 현대 기술, 총기, SF 요소는 금지한다.\n";
    out << "opening_location은 첫 장면의 실제 시작 지역명이다.\n";
    out << "first_clue는 정식 목표가 아니라, 장로를 만나기 전까지 추적할 첫 실마리다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << "[초기 고정 규칙]\n" << joinLimited(state.world.fixedRules, 8, "없음");
    out << "[시작 장비]\n" << joinItems(state.player.inventory, 8);
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"text\": \"5~8 Korean sentences\",\n";
    out << "  \"protagonist_wound\": \"string\",\n";
    out << "  \"personal_goal\": \"string\",\n";
    out << "  \"opening_location\": \"string\",\n";
    out << "  \"first_clue\": \"string\",\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    return out.str();
}

std::string PromptBuilder::buildNextEventPrompt(const GameState& state, const std::string& actionContext)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 다음 장면 디렉터다.\n";
    out << "현재 상태, 최근 사건, 플레이어의 직전 행동, 엔진 요청을 보고 다음 event_type을 고른다.\n";
    out << "전투 여부는 C++ 위험도 시스템이 결정한다. 엔진 요청이 combat을 금지하거나 강제하면 반드시 따른다.\n";
    out << "전투 계산과 보상 확정은 C++ 엔진이 담당하므로 수치 결과를 과장하지 않는다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << "[출력 길이 규칙]\n";
    out << "- scene_text는 2~4문장 또는 dialogue 4줄 이하로 쓴다.\n";
    out << "- decision_hint는 플레이어가 지금 판단할 상황만 1문장으로 쓴다.\n";
    out << "- next_objective와 memory_note는 각각 1문장으로 쓴다.\n";
    out << "- choices는 각 항목 25자 안팎의 짧은 행동문으로 쓴다.\n";
    out << "- UI에는 decision_hint만 중앙에 보이므로 군더더기 설명을 넣지 않는다.\n\n";
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
    out << "- 정보를 찾거나 말을 거는 행동은 보통 story, dialogue, item_gain, quest_update로 이어진다.\n";
    out << "- 위험한 장소로 이동하거나 위협을 건드리는 행동은 combat으로 이어질 수 있다.\n";
    out << "- 이미 전투 중인 장면이라면 적을 제압, 탈출, 협상했다는 근거가 없을 때 combat을 유지한다.\n";
    out << "- 같은 유형의 이벤트를 반복하더라도 최근 사건과 현재 실마리에 맞는 새 정보나 변화를 준다.\n";
    out << "- location은 이번 장면의 현재 지역명이다. 이동/진행이 일어나면 새 지역명으로 바꾸고, 같은 장소면 현재 위치를 유지한다.\n";
    out << "- combat을 선택할 때만 monster 객체를 name, description, hp, attack, defense로 채우고, 다른 event_type에서는 monster를 null로 둔다.\n";
    out << "- item_gain을 선택할 때는 item 객체를 채우고, 다른 event_type에서는 꼭 필요한 경우가 아니면 item을 null로 둔다.\n\n";
    out << "- base_candidate는 이번 장면의 현재 location을 기준으로 판단한다. 장소 이름에 마을/성채가 들어가도 폐허나 지하감옥이면 false다.\n";
    out << "- dialogue를 선택할 때 scene_text는 줄마다 '화자: 대사' 형태로 쓴다.\n";
    out << "- actionContext에 d12 결과가 있으면 그 결과를 이번 이벤트의 실제 판정 결과로 반영한다.\n\n";
    out << baseContext(state) << '\n';
    out << "[검색된 관련 기록]\n" << buildRagContext(state, actionContext) << '\n';
    out << "[직전 행동 컨텍스트]\n" << actionContext << "\n\n";
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"scene_text\": \"string\",\n";
    out << "  \"location\": \"string\",\n";
    out << "  \"event_type\": \"story\",\n";
    out << "  \"next_objective\": \"string\",\n";
    out << "  \"decision_hint\": \"string\",\n";
    out << "  \"base_candidate\": false,\n";
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
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 고유 행동 판정기다.\n";
    out << "플레이어의 고유 행동과 d12 결과를 보고 이번 행동의 직접 결과만 만든다.\n";
    out << "다음 장면 event_type 선택은 별도 단계에서 처리하므로 여기서는 행동 결과와 다음 이벤트 힌트만 쓴다.\n";
    out << "전투 계산, 거점 해방, 상점/여관/강화 처리는 C++ 엔진이 담당한다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << "[출력 길이 규칙]\n";
    out << "- result_text는 1~3문장으로만 쓴다.\n";
    out << "- next_event_hint와 memory_note는 각각 1문장으로 쓴다.\n";
    out << "- 한 문장에 장소, 감각, 결과를 모두 욱여넣지 말고 핵심 변화만 남긴다.\n\n";
    out << "[d12 판정 규칙]\n";
    out << "- 1~5 failure: 시도가 실패하거나 대가를 치른다.\n";
    out << "- 6~10 success: 의도한 결과를 적절히 얻는다.\n";
    out << "- 11~12 jackpot: 큰 이점을 얻고, 가능한 경우 item 객체를 반드시 채운다.\n\n";
    out << "[결과 규칙]\n";
    out << "- jackpot이면 플레이어가 실제로 손에 넣은 물건을 item에 채운다. 엔진은 이 아이템을 인벤토리에 저장한다.\n";
    out << "- jackpot 아이템은 현재 장소/행동과 자연스럽게 연결되어야 한다.\n";
    out << "- item을 줄 때는 정말 물건을 실제로 얻는 경우에만 채운다.\n";
    out << "- 진행용 물건은 전투 능력이나 회복 효과를 만들지 않는다.\n";
    out << "- 이미 언급된 소품을 다시 꺼내려면 이번 행동에서 실제로 조사, 사용, 상실, 전달, 해석되는 이유가 있어야 한다.\n";
    out << "- dialogue가 포함되면 result_text 안에서 줄마다 '화자: 대사' 형태로 쓴다.\n\n";
    out << "- base_candidate는 행동 결과 이후 현재 location이 안전한 생활권/정비 장소/장로가 있을 법한 장소일 때만 true다.\n";
    out << "- 지하감옥, 폐허, 버려진 성체, 적 본거지, 전투 지역, 위험한 통로, 사람이 살지 않는 장소는 반드시 false다.\n\n";
    out << baseContext(state) << '\n';
    out << "[검색된 관련 기록]\n" << buildRagContext(state, customInput + "\n" + diceOutcome) << '\n';
    out << "[고유 행동]\n" << customInput << "\n\n";
    out << "[d12 결과]\n" << diceOutcome << "\n\n";
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"result_text\": \"string\",\n";
    out << "  \"location\": \"string\",\n";
    out << "  \"stat_changes\": {\"hp\": 0, \"gold\": 0, \"exp\": 0},\n";
    out << "  \"item\": null,\n";
    out << "  \"next_event_hint\": \"string\",\n";
    out << "  \"base_candidate\": false,\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    return out.str();
}

std::string PromptBuilder::buildElderDialoguePrompt(const GameState& state)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 거점 장로 대화 장면 생성기다.\n";
    out << "이 장면은 반드시 거점의 장로와 나누는 단 한 번의 대화다.\n";
    out << "장로는 보스 정보를 처음으로 알려준다. 전투, 보상 지급, 아이템 획득은 만들지 않는다.\n";
    out << "보스 정보는 현재 퀘스트, 획득 아이템, 조우한 적 기록과 모순되면 안 된다.\n";
    out << "boss.location은 현재 거점이 아니라 앞으로 향할 최종 목적지다.\n";
    out << "boss.weakness는 기존 quest_item, 단서, 지역 전승 중 하나와 연결한다.\n";
    out << "dialogue는 줄마다 '장로: 대사' 또는 '플레이어: 대사' 형태로 쓰고, 총 3~6줄만 쓴다.\n";
    out << "대화 한 줄은 짧게 쓴다. 장로와 플레이어의 발화를 번갈아 배치한다.\n";
    out << "JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << baseContext(state) << '\n';
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"dialogue\": \"장로: string\\n플레이어: string\\n장로: string\",\n";
    out << "  \"boss\": {\n";
    out << "    \"name\": \"string\",\n";
    out << "    \"location\": \"string\",\n";
    out << "    \"weakness\": \"string\",\n";
    out << "    \"description\": \"string\"\n";
    out << "  },\n";
    out << "  \"quest_update\": \"string\",\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    return out.str();
}

std::string PromptBuilder::buildInitialWorldPrompt(const GameState& state)
{
    std::ostringstream out;
    out << "너는 중세 판타지 텍스트 RPG의 시작 상황 생성기다.\n";
    out << "프롤로그의 주인공 동기와 시작 위치를 바탕으로 게임의 첫 장면을 만든다.\n";
    out << "플레이어를 즉사시키거나 현대 기술, 총기, SF 요소를 넣지 않는다.\n";
    out << "location은 프롤로그 opening_location과 같거나 바로 이어지는 가까운 장소여야 한다.\n";
    out << "current_objective는 프롤로그 first_clue를 이어받되, 장로 대화 전에는 정식 목표가 아니라 실마리로 취급한다.\n";
    out << "scene_text는 2~4문장, decision_hint와 memory_note는 각각 1문장으로 쓴다.\n";
    out << "base_candidate는 시작 위치가 안전한 생활권/정비 장소/장로가 있을 법한 장소일 때만 true다. 폐허, 지하감옥, 버려진 장소, 적지면 false다.\n";
    out << "한국어 JSON 외 텍스트는 출력하지 않는다.\n\n";
    out << "[프롤로그]\n" << prologueContext(state.records.prologue) << '\n';
    out << "[출력 JSON]\n";
    out << "{\n";
    out << "  \"location\": \"string\",\n";
    out << "  \"scene_text\": \"string\",\n";
    out << "  \"current_objective\": \"string\",\n";
    out << "  \"decision_hint\": \"string\",\n";
    out << "  \"base_candidate\": false,\n";
    out << "  \"memory_note\": \"string\"\n";
    out << "}\n";
    return out.str();
}

} // namespace textrpg::llm::internals
