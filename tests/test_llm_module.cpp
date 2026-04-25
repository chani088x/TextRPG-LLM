#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "llm/LLMEventValidator.hpp"
#include "llm/LLMFallbackFactory.hpp"
#include "llm/LLMConfig.hpp"
#include "llm/LLMOutputParser.hpp"
#include "llm/LLMService.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace textrpg::llm;

namespace {

class FakeClient final : public ILLMClient {
public:
    explicit FakeClient(std::string response, bool shouldThrow = false)
        : response_(std::move(response))
        , shouldThrow_(shouldThrow)
    {
    }

    std::string generate(const std::string&) override
    {
        if (shouldThrow_) {
            throw std::runtime_error(response_);
        }
        return response_;
    }

private:
    std::string response_;
    bool shouldThrow_ = false;
};

GameEvent makeValidStoryEvent()
{
    GameEvent event;
    event.sceneText = "낡은 숲길 위로 안개가 내려앉았다.";
    event.eventType = EventType::Story;
    event.nextObjective = "숲길의 표식과 발자국을 확인한다.";
    event.decisionHint = "표식 조사는 단서를 줄 수 있지만 숲 안쪽의 위험을 늦게 발견할 수 있다.";
    event.choices = {
        "표식을 조사한다",
        "숲 안쪽으로 이동한다",
    };
    event.statChanges = {};
    event.memoryNote = "플레이어는 낡은 숲길에 도착했다.";
    return event;
}

std::string validStoryJson()
{
    return R"({
        "scene_text": "당신은 마을 북쪽의 낡은 숲길에 들어섰다.",
        "event_type": "story",
        "next_objective": "숲길에 남은 발자국과 부서진 표식을 확인한다.",
        "decision_hint": "표식을 조사하면 단서를 얻을 수 있지만 숲 안쪽의 움직임을 놓칠 수 있다.",
        "choices": [
            "부서진 표식을 자세히 살펴본다",
            "숲길을 따라 더 깊이 들어간다",
            "마을로 돌아간다"
        ],
        "monster": null,
        "item": null,
        "stat_changes": {
            "hp": 0,
            "gold": 0,
            "exp": 0
        },
        "memory_note": "플레이어는 마을 북쪽의 낡은 숲길에 들어섰다."
    })";
}

} // namespace

TEST_CASE("parser parses a valid story JSON")
{
    LLMOutputParser parser;
    const auto result = parser.parse(validStoryJson());

    REQUIRE(result.success);
    CHECK(result.event.eventType == EventType::Story);
    CHECK(result.event.nextObjective == "숲길에 남은 발자국과 부서진 표식을 확인한다.");
    CHECK(result.event.decisionHint == "표식을 조사하면 단서를 얻을 수 있지만 숲 안쪽의 움직임을 놓칠 수 있다.");
    CHECK(result.event.choices.size() == 3);
    CHECK_FALSE(result.event.monster.has_value());
    CHECK_FALSE(result.event.item.has_value());
}

TEST_CASE("parser extracts JSON from surrounding prose")
{
    LLMOutputParser parser;
    const auto result = parser.parse("설명문\n```json\n" + validStoryJson() + "\n```\n끝");

    REQUIRE(result.success);
    CHECK(result.event.memoryNote == "플레이어는 마을 북쪽의 낡은 숲길에 들어섰다.");
}

TEST_CASE("parser accepts combat without choices")
{
    LLMOutputParser parser;
    const auto result = parser.parse(R"({
        "scene_text": "늑대가 수풀에서 뛰쳐나와 길을 막았다.",
        "event_type": "combat",
        "next_objective": "늑대의 위협을 처리하고 수풀의 흔적을 확인한다.",
        "decision_hint": "전투는 전투 시스템에서 처리되며, 승리 후 흔적을 조사해야 한다.",
        "monster": {"name": "안개 늑대", "description": "짙은 안개를 두른 늑대.", "hp": 35, "attack": 9, "defense": 3},
        "item": null,
        "stat_changes": {"hp": -5, "exp": 10},
        "memory_note": "안개 늑대가 플레이어를 습격했다."
    })");

    REQUIRE(result.success);
    CHECK(result.event.eventType == EventType::Combat);
    CHECK(result.event.choices.empty());
    CHECK(result.event.statChanges.gold == 0);
    REQUIRE(result.event.monster.has_value());
    CHECK(result.event.monster->name == "안개 늑대");
}

TEST_CASE("parser rejects missing required fields")
{
    LLMOutputParser parser;
    const auto result = parser.parse(R"({
        "scene_text": "장면",
        "event_type": "story",
        "choices": ["간다", "쉰다"],
        "stat_changes": {"hp": 0, "gold": 0, "exp": 0}
    })");

    CHECK_FALSE(result.success);
    CHECK(result.errorMessage.find("memory_note") != std::string::npos);
}

TEST_CASE("parser rejects unknown event types")
{
    LLMOutputParser parser;
    const auto result = parser.parse(R"({
        "scene_text": "용이 하늘을 찢었다.",
        "event_type": "dragon_attack_super_legend",
        "choices": ["본다", "도망친다"],
        "monster": null,
        "item": null,
        "stat_changes": {"hp": 0, "gold": 0, "exp": 0},
        "memory_note": "이상한 이벤트가 생성되었다."
    })");

    CHECK_FALSE(result.success);
    CHECK(result.errorMessage.find("event_type") != std::string::npos);
}

TEST_CASE("validator accepts 2 to 4 choices")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();

    auto result = validator.validate(event);

    CHECK(result.valid);
    CHECK_FALSE(result.repaired);
}

TEST_CASE("validator rejects one choice")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.choices = {"앞으로 간다"};

    auto result = validator.validate(event);

    CHECK_FALSE(result.valid);
}

TEST_CASE("validator truncates five choices")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.choices = {
        "첫 번째 단서를 자세히 조사한다",
        "두 번째 길로 조심스럽게 이동한다",
        "세 번째 흔적을 따라 숲으로 간다",
        "네 번째 소리를 듣고 몸을 숨긴다",
        "다섯 번째 물건을 챙기고 돌아간다",
    };

    auto result = validator.validate(event);

    REQUIRE(result.valid);
    CHECK(result.repaired);
    CHECK(result.event.choices.size() == 4);
}

TEST_CASE("validator clamps stat changes")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.statChanges.hp = -999;
    event.statChanges.gold = 9999;
    event.statChanges.exp = 9999;

    auto result = validator.validate(event);

    REQUIRE(result.valid);
    CHECK(result.event.statChanges.hp == -30);
    CHECK(result.event.statChanges.gold == 100);
    CHECK(result.event.statChanges.exp == 50);
}

TEST_CASE("validator clamps monster stats")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.eventType = EventType::Combat;
    event.monster = Monster {"거대 늑대", "비정상적으로 강한 늑대", 999, 999, 999};

    auto result = validator.validate(event);

    REQUIRE(result.valid);
    CHECK(result.repaired);
    REQUIRE(result.event.monster.has_value());
    CHECK(result.event.monster->hp == 200);
    CHECK(result.event.monster->attack == 30);
    CHECK(result.event.monster->defense == 20);
}

TEST_CASE("validator rejects combat without monster")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.eventType = EventType::Combat;
    event.monster.reset();

    auto result = validator.validate(event);

    CHECK_FALSE(result.valid);
}

TEST_CASE("validator accepts combat without choices")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.eventType = EventType::Combat;
    event.monster = Monster {"안개 늑대", "안개 속에서 달려드는 늑대", 35, 9, 3};
    event.choices.clear();

    auto result = validator.validate(event);

    REQUIRE(result.valid);
    CHECK(result.event.choices.empty());
}

TEST_CASE("validator rejects item gain without item")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.eventType = EventType::ItemGain;
    event.item.reset();

    auto result = validator.validate(event);

    CHECK_FALSE(result.valid);
}

TEST_CASE("validator infers missing objective and decision hint")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.nextObjective.clear();
    event.decisionHint.clear();

    auto result = validator.validate(event);

    REQUIRE(result.valid);
    CHECK(result.repaired);
    CHECK_FALSE(result.event.nextObjective.empty());
    CHECK_FALSE(result.event.decisionHint.empty());
}

TEST_CASE("validator replaces underspecified choices")
{
    LLMEventValidator validator;
    auto event = makeValidStoryEvent();
    event.choices = {"간다", "쉰다"};

    auto result = validator.validate(event);

    REQUIRE(result.valid);
    CHECK(result.repaired);
    CHECK(result.event.choices.front() != "간다");
    CHECK(result.event.choices.front().size() >= 15);
}

TEST_CASE("fallback factory creates safe story event")
{
    LLMFallbackFactory fallbackFactory;
    const auto event = fallbackFactory.createSafeEvent("bad json");

    CHECK(event.usedFallback);
    CHECK(event.eventType == EventType::Story);
    CHECK_FALSE(event.nextObjective.empty());
    CHECK_FALSE(event.decisionHint.empty());
    CHECK(event.choices.size() == 3);
    CHECK(event.statChanges.hp == 0);
    CHECK(event.statChanges.gold == 0);
    CHECK(event.statChanges.exp == 0);
}

TEST_CASE("config loader supports nothink alias")
{
    const auto path = std::filesystem::temp_directory_path() / "llm_config_nothink_alias_test.toml";
    {
        std::ofstream output(path);
        output << "[llm]\n";
        output << "think = true\n";
        output << "nothink = true\n";
    }

    const auto config = LLMConfigLoader::load(path);
    std::filesystem::remove(path);

    CHECK_FALSE(config.ollama.think);
}

TEST_CASE("service falls back on malformed JSON")
{
    auto client = std::make_shared<FakeClient>("not json");
    LLMService service(client);

    GameState state;
    state.turnNumber = 3;
    const auto event = service.generateEvent(state, "북쪽으로 간다");

    CHECK(event.usedFallback);
    CHECK(event.eventType == EventType::Story);
}

TEST_CASE("service falls back on transport failure")
{
    auto client = std::make_shared<FakeClient>("connection failed", true);
    LLMService service(client);

    GameState state;
    const auto event = service.generateEvent(state, "문을 연다");

    CHECK(event.usedFallback);
    CHECK(event.validationNotes.front() == "connection failed");
}
