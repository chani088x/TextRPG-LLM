#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace textrpg::llm {

namespace ids {

// 팀원이 LLM JSON 타입을 확장할 때는 이 ID 목록과 README의 확장 가이드를 같이 갱신한다.
namespace event {
inline constexpr const char* Story = "story";
inline constexpr const char* Combat = "combat";
inline constexpr const char* ItemGain = "item_gain";
inline constexpr const char* StatChange = "stat_change";
inline constexpr const char* Dialogue = "dialogue";
inline constexpr const char* QuestUpdate = "quest_update";
inline constexpr const char* Rest = "rest";
inline constexpr const char* GameEnd = "game_end";
} // namespace event

namespace item {
inline constexpr const char* Weapon = "weapon";
inline constexpr const char* Armor = "armor";
inline constexpr const char* Consumable = "consumable";
inline constexpr const char* QuestItem = "quest_item";
} // namespace item

namespace dice {
inline constexpr const char* Failure = "failure";
inline constexpr const char* Success = "success";
inline constexpr const char* Jackpot = "jackpot";
} // namespace dice

} // namespace ids

enum class EventRoll {
    NonCombat = 0,
    Combat = 1
};

enum class CombatChoice {
    Attack,
    Skill,
    Item,
    Custom
};

enum class StoryChoice {
    Advance,
    Investigate
};

// LLM은 몬스터 후보만 만든다. 실제 전투 결과 계산은 전투 시스템의 책임이다.
struct Monster {
    std::string name;
    std::string description;
    int hp = 1;
    int attack = 1;
    int defense = 0;
};

// item_gain 이벤트에서 엔진이 인벤토리에 넘길 수 있는 최소 아이템 정보다.
struct Item {
    std::string name;
    std::string type = ids::item::Consumable;
    std::string description;
    int value = 0;
};

// LLM이 제안할 수 있는 상태 변화다. 파싱 단계에서 안전 범위로 제한한다.
struct StatChanges {
    int hp = 0;
    int gold = 0;
    int exp = 0;
};

struct GeneratedNPC {
    std::string name;
    std::string personality;
    std::string speechStyle;
    int affinity = 0;
    bool isMet = false;
};

struct ActionResult {
    std::string resultText;
    std::string resultType;
    std::string itemName;
    std::string itemDescription;
    int hpDelta = 0;
    int goldDelta = 0;
    int expDelta = 0;
    bool usedFallback = false;
    std::vector<std::string> notes;
};

struct InitialWorld {
    std::string location;
    std::string sceneText;
    std::string currentObjective;
    std::string decisionHint;
    std::string memoryNote;
    bool usedFallback = false;
    std::vector<std::string> notes;
};

// GameEngine에 반환되는 LLM 모듈의 최종 산출물이다.
// 이 구조체를 만들기 전에는 항상 parse와 validate 단계를 거쳐야 한다.
struct GameEvent {
    std::string sceneText;
    std::string location;
    std::string eventType = ids::event::Story;
    std::string nextObjective;
    std::string decisionHint;
    std::vector<std::string> choices;
    std::optional<Monster> monster;
    std::optional<Item> item;
    std::optional<GeneratedNPC> newNPC;
    StatChanges statChanges;
    std::string memoryNote;
    bool usedFallback = false;
    std::vector<std::string> validationNotes;
};

// LLM에게 공개해도 되는 플레이어 상태 요약이다.
// 전체 Player 클래스 대신 필요한 값만 복사해 context를 작게 유지한다.
struct PlayerSnapshot {
    int hp = 100;
    int maxHp = 100;
    int level = 1;
    int attack = 10;
    int defense = 5;
    int gold = 0;
    int exp = 0;
    std::vector<std::string> inventory;
};

// 현재 위치와 목표처럼 스토리 생성에 필요한 세계 상태 요약이다.
struct WorldState {
    std::string location = "이름 없는 길";
    std::string currentObjective = "다음 단서를 찾는다";
    std::string decisionHint = "현재 장면을 보고 다음 행동의 위험과 보상을 판단한다";
    std::vector<std::string> fixedRules;
};

// 전체 로그가 아니라 최근 사건과 중요한 선택만 LLM에게 전달한다.
struct StoryMemory {
    std::vector<std::string> recentEvents;
    std::vector<std::string> importantChoices;
};

// LLM 엔진이 생성과 상태 반영에 사용하는 최소 게임 상태다.
struct GameState {
    int turnNumber = 0;
    std::string currentScene;
    PlayerSnapshot player;
    WorldState world;
    StoryMemory memory;
};

inline bool isKnownEventType(const std::string& value)
{
    return value == ids::event::Story
        || value == ids::event::Combat
        || value == ids::event::ItemGain
        || value == ids::event::StatChange
        || value == ids::event::Dialogue
        || value == ids::event::QuestUpdate
        || value == ids::event::Rest
        || value == ids::event::GameEnd;
}

inline std::string normalizeEventType(const std::string& value, const std::string& fallback = ids::event::Story)
{
    return isKnownEventType(value) ? value : fallback;
}

inline std::string eventTypeToString(const std::string& type)
{
    return type;
}

inline bool isKnownItemType(const std::string& value)
{
    return value == ids::item::Weapon
        || value == ids::item::Armor
        || value == ids::item::Consumable
        || value == ids::item::QuestItem;
}

inline std::string normalizeItemType(const std::string& value)
{
    return isKnownItemType(value) ? value : ids::item::Consumable;
}

inline std::string itemTypeToString(const std::string& type)
{
    return type;
}

inline bool isKnownDiceOutcome(const std::string& value)
{
    return value == ids::dice::Failure
        || value == ids::dice::Success
        || value == ids::dice::Jackpot;
}

inline std::string normalizeDiceOutcome(const std::string& value)
{
    return isKnownDiceOutcome(value) ? value : ids::dice::Failure;
}

inline std::string diceOutcomeToString(const std::string& outcome)
{
    return outcome;
}

inline std::string diceOutcomeToKorean(const std::string& outcome)
{
    if (outcome == ids::dice::Success) {
        return "성공";
    }
    if (outcome == ids::dice::Jackpot) {
        return "초대박";
    }
    return "실패";
}

inline bool isFailureOutcome(const std::string& outcome)
{
    return outcome == ids::dice::Failure;
}

inline std::vector<std::string> eventTypeIds()
{
    return {
        ids::event::Story,
        ids::event::Combat,
        ids::event::ItemGain,
        ids::event::StatChange,
        ids::event::Dialogue,
        ids::event::QuestUpdate,
        ids::event::Rest,
        ids::event::GameEnd,
    };
}

inline std::vector<std::string> itemTypeIds()
{
    return {
        ids::item::Weapon,
        ids::item::Armor,
        ids::item::Consumable,
        ids::item::QuestItem,
    };
}

inline std::vector<std::string> diceOutcomeIds()
{
    return {
        ids::dice::Failure,
        ids::dice::Success,
        ids::dice::Jackpot,
    };
}

inline std::string joinIds(const std::vector<std::string>& values, const std::string& separator = " | ")
{
    std::string joined;
    for (const auto& value : values) {
        if (!joined.empty()) {
            joined += separator;
        }
        joined += value;
    }
    return joined;
}

inline int clampInt(int value, int low, int high)
{
    return std::max(low, std::min(value, high));
}

} // namespace textrpg::llm
