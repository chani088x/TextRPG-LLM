#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace textrpg::llm {

// LLM JSON의 event_type 문자열과 1:1로 맞춰지는 이벤트 종류다.
// Invalid는 파싱/검증 실패를 안전하게 표현하기 위한 내부 값이다.
enum class EventType {
    Story,
    Combat,
    ItemGain,
    StatChange,
    Dialogue,
    QuestUpdate,
    Rest,
    GameEnd,
    Invalid
};

// LLM JSON의 item.type 문자열과 맞춰지는 아이템 종류다.
enum class ItemType {
    Weapon,
    Armor,
    Consumable,
    QuestItem,
    Invalid
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
    ItemType type = ItemType::Consumable;
    std::string description;
    int value = 0;
};

// LLM이 제안할 수 있는 상태 변화다. 최종 적용 전 Validator에서 범위를 제한한다.
struct StatChanges {
    int hp = 0;
    int gold = 0;
    int exp = 0;
};

// GameEngine에 반환되는 LLM 모듈의 최종 산출물이다.
// 이 구조체를 만들기 전에는 항상 parse와 validate 단계를 거쳐야 한다.
struct GameEvent {
    std::string sceneText;
    EventType eventType = EventType::Story;
    std::string nextObjective;
    std::string decisionHint;
    std::vector<std::string> choices;
    std::optional<Monster> monster;
    std::optional<Item> item;
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

// LLM 모듈 입력용 상태 스냅샷이다. 이 모듈이 GameState를 직접 수정하지는 않는다.
struct GameState {
    int turnNumber = 0;
    std::string currentScene;
    PlayerSnapshot player;
    WorldState world;
    StoryMemory memory;
};

// ContextBuilder가 긴 상태를 얼마나 잘라낼지 정하는 값이다.
struct PromptSettings {
    std::size_t maxRecentEvents = 5;
    std::size_t maxInventoryItems = 8;
    std::string language = "ko";
};

// LLM 응답을 게임 규칙 안으로 묶기 위한 밸런스 제한값이다.
struct ValidationConfig {
    std::size_t minChoices = 2;
    std::size_t maxChoices = 4;
    int maxMonsterHp = 200;
    int maxMonsterAttack = 30;
    int maxMonsterDefense = 20;
    int maxHpDelta = 30;
    int maxGoldReward = 100;
    int maxExpReward = 50;
    int maxItemValue = 100;
};

inline std::string eventTypeToString(EventType type)
{
    switch (type) {
    case EventType::Story:
        return "story";
    case EventType::Combat:
        return "combat";
    case EventType::ItemGain:
        return "item_gain";
    case EventType::StatChange:
        return "stat_change";
    case EventType::Dialogue:
        return "dialogue";
    case EventType::QuestUpdate:
        return "quest_update";
    case EventType::Rest:
        return "rest";
    case EventType::GameEnd:
        return "game_end";
    case EventType::Invalid:
        return "invalid";
    }

    return "invalid";
}

inline EventType eventTypeFromString(const std::string& value)
{
    if (value == "story") {
        return EventType::Story;
    }
    if (value == "combat") {
        return EventType::Combat;
    }
    if (value == "item_gain") {
        return EventType::ItemGain;
    }
    if (value == "stat_change") {
        return EventType::StatChange;
    }
    if (value == "dialogue") {
        return EventType::Dialogue;
    }
    if (value == "quest_update") {
        return EventType::QuestUpdate;
    }
    if (value == "rest") {
        return EventType::Rest;
    }
    if (value == "game_end") {
        return EventType::GameEnd;
    }
    return EventType::Invalid;
}

inline std::string itemTypeToString(ItemType type)
{
    switch (type) {
    case ItemType::Weapon:
        return "weapon";
    case ItemType::Armor:
        return "armor";
    case ItemType::Consumable:
        return "consumable";
    case ItemType::QuestItem:
        return "quest_item";
    case ItemType::Invalid:
        return "invalid";
    }

    return "invalid";
}

inline ItemType itemTypeFromString(const std::string& value)
{
    if (value == "weapon") {
        return ItemType::Weapon;
    }
    if (value == "armor") {
        return ItemType::Armor;
    }
    if (value == "consumable") {
        return ItemType::Consumable;
    }
    if (value == "quest_item") {
        return ItemType::QuestItem;
    }
    return ItemType::Invalid;
}

inline int clampInt(int value, int low, int high)
{
    return std::max(low, std::min(value, high));
}

} // namespace textrpg::llm
