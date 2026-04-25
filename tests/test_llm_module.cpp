#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "combat/CombatSystem.hpp"
#include "llm/LLM.hpp"

using namespace textrpg::llm;

TEST_CASE("LLM parser reads a story event from raw text")
{
    const auto event = LLM::parseEvent(R"(prefix {
        "scene_text": "당신은 부서진 표식을 발견했다.",
        "event_type": "story",
        "next_objective": "표식 주변의 발자국을 확인한다.",
        "decision_hint": "단서를 얻으려면 조사해야 하지만 시간이 지날 수 있다.",
        "choices": [
            "부서진 표식을 자세히 조사한다",
            "발자국을 따라 숲 안쪽으로 이동한다"
        ],
        "monster": null,
        "item": null,
        "stat_changes": {"hp": 0, "gold": 0, "exp": 0},
        "memory_note": "플레이어는 부서진 표식을 발견했다."
    } suffix)");

    CHECK_FALSE(event.usedFallback);
    CHECK(event.eventType == EventType::Story);
    CHECK(event.sceneText == "당신은 부서진 표식을 발견했다.");
    CHECK(event.choices.size() == 2);
}

TEST_CASE("LLM parser defaults combat monster stats")
{
    const auto event = LLM::parseEvent(R"({
        "scene_text": "검은 갑옷의 적이 길을 막았다.",
        "event_type": "combat",
        "next_objective": "적을 처리하고 길 끝의 흔적을 확인한다.",
        "decision_hint": "전투는 빠르지만 피해를 입을 수 있다.",
        "choices": [],
        "monster": {"name": "검은 기사", "description": "녹슨 검을 든 적."},
        "item": null,
        "stat_changes": {"hp": -999, "gold": 999, "exp": 999},
        "memory_note": "검은 기사가 나타났다."
    })");

    REQUIRE(event.monster.has_value());
    CHECK(event.monster->name == "검은 기사");
    CHECK(event.monster->hp == 10);
    CHECK(event.monster->attack == 1);
    CHECK(event.statChanges.hp == -30);
    CHECK(event.statChanges.gold == 100);
    CHECK(event.statChanges.exp == 50);
    CHECK(event.choices.empty());
}

TEST_CASE("LLM parser falls back without JSON")
{
    const auto event = LLM::parseEvent("not json");

    CHECK(event.usedFallback);
    CHECK(event.eventType == EventType::Story);
    CHECK(event.choices.size() == 3);
}

TEST_CASE("combat module uses only attack with default stats")
{
    textrpg::combat::CombatSystem combatSystem;

    const auto result = combatSystem.run(
        textrpg::combat::makeDefaultPlayer(),
        textrpg::combat::makeDefaultMonster("훈련용 몬스터"));

    CHECK(result.winner == textrpg::combat::CombatWinner::Player);
    CHECK(result.player.hp == 998);
    CHECK(result.player.attack == 9);
    CHECK(result.monster.hp == 0);
    CHECK(result.monster.attack == 1);
    REQUIRE(result.turns.size() == 3);
    CHECK(result.turns[0].skill == textrpg::combat::SkillType::Attack);
    CHECK(result.turns[1].skill == textrpg::combat::SkillType::Attack);
    CHECK(result.turns[2].skill == textrpg::combat::SkillType::Attack);
}
