#pragma once

#include <algorithm>
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
    Item,
    Custom
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

struct Quest {
    std::string title;
    std::string description;
    std::string reward;
};

struct InitialWorld {
    std::string location;
    std::string sceneText;
    std::string currentObjective;
    std::string decisionHint;
    bool baseCandidate = false;
    std::string memoryNote;
    bool usedFallback = false;
    std::vector<std::string> notes;
};

// 주인공 입장에서 생성되는 시작 서사다.
// 한 번 생성되면 기록 JSON에 저장되어 이후 장면 생성의 개인적 동기로 쓰인다.
struct Prologue {
    bool generated = false;
    std::string text;
    std::string protagonistWound;
    std::string personalGoal;
    std::string openingLocation;
    std::string firstObjective;
    std::string memoryNote;
    bool usedFallback = false;
    std::vector<std::string> validationNotes;
};

// GameEngine에 반환되는 LLM 모듈의 최종 산출물이다.
// 이 구조체를 만들기 전에는 항상 parse와 validate 단계를 거쳐야 한다.
struct GameEvent {
    std::string sceneText;
    std::string location;
    std::string eventType = ids::event::Story;
    std::string nextObjective;
    std::string decisionHint;
    bool baseCandidate = false;
    std::vector<std::string> choices;
    std::optional<Monster> monster;
    std::optional<Item> item;
    StatChanges statChanges;
    std::string memoryNote;
    bool usedFallback = false;
    std::vector<std::string> validationNotes;
};

// 고유 행동 판정의 직접 결과다.
// 다음 장면 선택(GameEvent)과 분리해서, "행동 결과 적용 후 다음 이벤트 선택" 흐름을 만든다.
struct ActionResult {
    std::string resultText;
    std::string location;
    StatChanges statChanges;
    std::optional<Item> item;
    std::string nextEventHint;
    bool baseCandidate = false;
    std::string memoryNote;
    bool usedFallback = false;
    std::vector<std::string> validationNotes;
};

struct BossInfo {
    bool known = false;
    std::string name;
    std::string location;
    std::string weakness;
    std::string description;
};

// 거점 장로와의 1회성 대화 결과다.
// LLM은 후보 정보를 만들고, C++ 엔진이 기록에 저장하는 순간 확정된다.
struct ElderDialogueResult {
    std::string dialogue;
    BossInfo boss;
    std::string questUpdate;
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
    std::vector<Item> inventory;
};

// 현재 위치와 목표처럼 스토리 생성에 필요한 세계 상태 요약이다.
struct WorldState {
    std::string location = "이름 없는 길";
    std::string currentObjective = "다음 단서를 찾는다";
    std::string decisionHint = "현재 장면을 보고 다음 행동의 위험과 보상을 판단한다";
    bool baseCandidate = false;
    std::vector<std::string> fixedRules;
};

// 전체 로그가 아니라 최근 사건과 중요한 선택만 LLM에게 전달한다.
struct StoryMemory {
    std::vector<std::string> recentEvents;
    std::vector<std::string> importantChoices;
};

struct GameRecords {
    struct EventRecord {
        int turnNumber = 0;
        std::string eventType = ids::event::Story;
        std::string eventLabel;
        std::string summary;
    };

    struct SnapshotState {
        bool saved = false;
        int turnNumber = 1;
        std::string currentScene;
        PlayerSnapshot player;
        WorldState world;
        StoryMemory memory;
    };

    SnapshotState snapshot;
    std::vector<std::string> questLog;
    std::vector<Item> obtainedItems;
    std::vector<Monster> encounteredEnemies;
    std::vector<EventRecord> eventHistory;
    Prologue prologue;
    struct DangerState {
        int level = 0;
        int lastIncrease = 0;
        int threshold = 10;
    } danger;
    struct BaseState {
        bool unlocked = false;
        std::string location;
        std::vector<std::string> features;
        std::vector<std::string> declinedLocations;
    } base;
    struct ElderState {
        bool introduced = false;
        bool talked = false;
    } elder;
    BossInfo boss;
};

// LLM 엔진이 생성과 상태 반영에 사용하는 최소 게임 상태다.
struct GameState {
    int turnNumber = 0;
    std::string currentScene;
    PlayerSnapshot player;
    WorldState world;
    StoryMemory memory;
    GameRecords records;
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
    if (type == ids::event::Story) {
        return "장면";
    }
    if (type == ids::event::Combat) {
        return "전투";
    }
    if (type == ids::event::ItemGain) {
        return "발견";
    }
    if (type == ids::event::Dialogue) {
        return "대화";
    }
    if (type == ids::event::QuestUpdate) {
        return "퀘스트";
    }
    if (type == ids::event::Rest) {
        return "휴식";
    }
    if (type == ids::event::GameEnd) {
        return "엔딩";
    }
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

inline bool containsAnyKeyword(const std::string& text, const std::vector<std::string>& keywords)
{
    for (const auto& keyword : keywords) {
        if (!keyword.empty() && text.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline bool isQuestLikeItemName(const std::string& name)
{
    return containsAnyKeyword(name, {
        "인장",
        "증표",
        "표식",
        "열쇠",
        "문서",
        "단서",
    });
}

inline bool descriptionImpliesPower(const std::string& description)
{
    return containsAnyKeyword(description, {
        "공격",
        "방어",
        "회복",
        "강화",
        "전투",
        "피해",
        "무적",
        "초월",
        "강력",
        "개쩌",
    });
}

inline void sanitizeItem(Item& item, std::vector<std::string>* notes = nullptr)
{
    const auto rawType = item.type;
    item.type = normalizeItemType(item.type);
    if (notes && rawType != item.type) {
        notes->push_back("item type was invalid and repaired");
    }

    if (isQuestLikeItemName(item.name) && item.type != ids::item::QuestItem) {
        item.type = ids::item::QuestItem;
        if (notes) {
            notes->push_back("quest-like item type was forced to quest_item");
        }
    }

    if (item.type == ids::item::QuestItem) {
        if (item.value != 0 && notes) {
            notes->push_back("quest_item value was forced to 0");
        }
        item.value = 0;
        if (descriptionImpliesPower(item.description)) {
            item.description = "진행 단서로 쓰이는 평범한 물건이다.";
            if (notes) {
                notes->push_back("quest_item power-like description was repaired");
            }
        }
    } else if (item.type == ids::item::Consumable) {
        item.value = std::max(0, std::min(item.value, 20));
    } else {
        item.value = std::max(0, std::min(item.value, 30));
    }
}

inline std::string itemTypeToString(const std::string& type)
{
    if (type == ids::item::Weapon) {
        return "무기";
    }
    if (type == ids::item::Armor) {
        return "방어구";
    }
    if (type == ids::item::Consumable) {
        return "소모품";
    }
    if (type == ids::item::QuestItem) {
        return "단서";
    }
    return type;
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
