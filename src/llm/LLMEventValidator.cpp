#include "llm/LLMEventValidator.hpp"

#include <algorithm>
#include <utility>

namespace textrpg::llm {
namespace {

std::string inferNextObjective(const GameEvent& event)
{
    switch (event.eventType) {
    case EventType::Combat:
        if (event.monster.has_value()) {
            return event.monster->name + "의 위협을 처리하고 남은 흔적을 확인한다";
        }
        return "눈앞의 위협을 처리하고 안전한 다음 경로를 찾는다";
    case EventType::ItemGain:
        if (event.item.has_value()) {
            return event.item->name + "의 용도를 확인하고 다음 단서를 찾는다";
        }
        return "새로 얻은 물건의 의미를 확인한다";
    case EventType::QuestUpdate:
        return "갱신된 단서를 따라 다음 목적지를 확인한다";
    case EventType::Dialogue:
        return "대화에서 얻은 정보를 바탕으로 다음 행동을 정한다";
    case EventType::Rest:
        return "회복을 마치고 이동 방향을 정한다";
    case EventType::GameEnd:
        return "여정을 마무리한다";
    case EventType::Story:
    case EventType::StatChange:
    case EventType::Invalid:
        return "현재 장면의 단서를 확인하고 다음 행동을 정한다";
    }

    return "현재 장면의 단서를 확인하고 다음 행동을 정한다";
}

std::string inferDecisionHint(const GameEvent& event)
{
    switch (event.eventType) {
    case EventType::Combat:
        return "전투를 밀어붙이면 단서를 빨리 얻을 수 있지만 HP 손실 위험이 있다";
    case EventType::ItemGain:
        return "아이템을 바로 활용할지, 먼저 안전을 확보할지 정해야 한다";
    case EventType::QuestUpdate:
        return "새 단서를 추적하는 속도와 현재 위치의 안전 사이에서 선택해야 한다";
    case EventType::Dialogue:
        return "상대의 정보를 믿고 움직일지, 추가 확인을 할지 판단해야 한다";
    case EventType::Rest:
        return "회복으로 시간을 쓸지, 위험을 감수하고 전진할지 선택해야 한다";
    case EventType::GameEnd:
        return "마지막 선택이 결말을 확정한다";
    case EventType::Story:
    case EventType::StatChange:
    case EventType::Invalid:
        return "선택지는 정보 확보, 안전 확보, 빠른 진행 중 무엇을 우선할지 가른다";
    }

    return "선택지는 정보 확보, 안전 확보, 빠른 진행 중 무엇을 우선할지 가른다";
}

std::vector<std::string> inferChoices(const GameEvent& event)
{
    switch (event.eventType) {
    case EventType::Combat: {
        const auto enemy = event.monster.has_value() ? event.monster->name : std::string("눈앞의 적");
        return {
            enemy + "에게 정면으로 맞서 위협을 끝낸다",
            "거리를 벌려 상처를 줄이고 전황을 살핀다",
            "주변 지형을 이용해 우회할 길을 찾는다",
        };
    }
    case EventType::ItemGain:
        return {
            "새로 얻은 물건을 자세히 조사한다",
            "물건을 챙기고 주변의 추가 단서를 찾는다",
            "위험을 피하기 위해 곧장 이동한다",
        };
    case EventType::Dialogue:
        return {
            "상대에게 자세한 정보를 더 캐묻는다",
            "대화 내용을 믿고 다음 장소로 이동한다",
            "거짓말 가능성을 확인할 증거를 찾는다",
        };
    case EventType::Rest:
        return {
            "충분히 쉬어 체력을 더 회복한다",
            "짧게 정비한 뒤 바로 이동한다",
            "주변을 살펴 안전한 야영지인지 확인한다",
        };
    case EventType::QuestUpdate:
    case EventType::Story:
    case EventType::StatChange:
    case EventType::GameEnd:
    case EventType::Invalid:
        return {
            "눈앞의 단서를 자세히 조사한다",
            "안전을 우선하며 다른 경로를 확인한다",
            "시간을 아끼기 위해 목표 방향으로 전진한다",
        };
    }

    return {
        "눈앞의 단서를 자세히 조사한다",
        "안전을 우선하며 다른 경로를 확인한다",
        "시간을 아끼기 위해 목표 방향으로 전진한다",
    };
}

bool hasUnderspecifiedChoices(const std::vector<std::string>& choices)
{
    return std::any_of(choices.begin(), choices.end(), [](const std::string& choice) {
        return choice.size() < 15;
    });
}

} // namespace

LLMEventValidator::LLMEventValidator(ValidationConfig config)
    : config_(std::move(config))
{
}

ValidationResult LLMEventValidator::validate(const GameEvent& event) const
{
    ValidationResult result;
    result.event = event;

    // 구조가 깨진 응답은 고쳐 쓰지 않고 fallback으로 넘긴다.
    if (result.event.eventType == EventType::Invalid) {
        result.messages.push_back("event_type is invalid");
        return result;
    }

    if (result.event.sceneText.empty()) {
        result.messages.push_back("scene_text is empty");
        return result;
    }

    if (result.event.memoryNote.empty()) {
        result.messages.push_back("memory_note is empty");
        return result;
    }

    const bool choicesRequired = result.event.eventType != EventType::Combat;
    if (!choicesRequired) {
        result.event.choices.clear();
    }

    if (choicesRequired && result.event.choices.size() < config_.minChoices) {
        result.messages.push_back("choices count is below minimum");
        return result;
    }

    // 선택지가 너무 많은 것은 앞쪽만 사용해 repair할 수 있다.
    if (choicesRequired && result.event.choices.size() > config_.maxChoices) {
        result.event.choices.resize(config_.maxChoices);
        result.repaired = true;
        result.messages.push_back("choices count exceeded maximum and was truncated");
    }

    if (choicesRequired && std::any_of(result.event.choices.begin(), result.event.choices.end(), [](const std::string& choice) {
            return choice.empty();
        })) {
        result.messages.push_back("choices contain an empty string");
        return result;
    }

    if (choicesRequired && hasUnderspecifiedChoices(result.event.choices)) {
        result.event.choices = inferChoices(result.event);
        result.repaired = true;
        result.messages.push_back("underspecified choices were replaced");
    }

    if (result.event.nextObjective.empty()) {
        result.event.nextObjective = inferNextObjective(result.event);
        result.repaired = true;
        result.messages.push_back("next_objective was inferred");
    }

    if (result.event.decisionHint.empty()) {
        result.event.decisionHint = inferDecisionHint(result.event);
        result.repaired = true;
        result.messages.push_back("decision_hint was inferred");
    }

    // event_type과 optional payload의 관계는 엔진 계약을 깨기 쉬워 엄격히 본다.
    if (result.event.eventType == EventType::Combat && !result.event.monster.has_value()) {
        result.messages.push_back("combat event requires monster");
        return result;
    }

    if (result.event.eventType != EventType::Combat && result.event.monster.has_value()) {
        result.messages.push_back("monster must be null unless event_type is combat");
        return result;
    }

    if (result.event.eventType == EventType::ItemGain && !result.event.item.has_value()) {
        result.messages.push_back("item_gain event requires item");
        return result;
    }

    if (result.event.monster.has_value()) {
        auto& monster = result.event.monster.value();
        const auto originalHp = monster.hp;
        const auto originalAttack = monster.attack;
        const auto originalDefense = monster.defense;

        // 수치 초과는 fallback 대신 clamp한다. 밸런스 최종 책임은 엔진 쪽에 둔다.
        monster.hp = clampInt(monster.hp, 1, config_.maxMonsterHp);
        monster.attack = clampInt(monster.attack, 1, config_.maxMonsterAttack);
        monster.defense = clampInt(monster.defense, 0, config_.maxMonsterDefense);

        if (monster.hp != originalHp || monster.attack != originalAttack || monster.defense != originalDefense) {
            result.repaired = true;
            result.messages.push_back("monster stats were clamped");
        }
    }

    if (result.event.item.has_value()) {
        auto& item = result.event.item.value();
        const auto originalValue = item.value;
        item.value = clampInt(item.value, 0, config_.maxItemValue);
        if (item.value != originalValue) {
            result.repaired = true;
            result.messages.push_back("item value was clamped");
        }
    }

    const auto originalHp = result.event.statChanges.hp;
    const auto originalGold = result.event.statChanges.gold;
    const auto originalExp = result.event.statChanges.exp;

    // LLM이 플레이어를 즉사시키거나 과도한 보상을 주지 못하도록 범위를 제한한다.
    result.event.statChanges.hp = clampInt(result.event.statChanges.hp, -config_.maxHpDelta, config_.maxHpDelta);
    result.event.statChanges.gold = clampInt(result.event.statChanges.gold, 0, config_.maxGoldReward);
    result.event.statChanges.exp = clampInt(result.event.statChanges.exp, 0, config_.maxExpReward);

    if (result.event.statChanges.hp != originalHp
        || result.event.statChanges.gold != originalGold
        || result.event.statChanges.exp != originalExp) {
        result.repaired = true;
        result.messages.push_back("stat_changes were clamped");
    }

    result.valid = true;
    result.event.validationNotes = result.messages;
    return result;
}

} // namespace textrpg::llm
