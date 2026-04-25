#pragma once

#include <string>
#include <utility>
#include <vector>

namespace textrpg::combat {

enum class SkillType {
    Attack
};

enum class CombatActor {
    Player,
    Monster
};

enum class CombatWinner {
    Player,
    Monster
};

struct Combatant {
    std::string name;
    int hp = 1;
    int attack = 1;
};

struct CombatTurn {
    CombatActor actor = CombatActor::Player;
    SkillType skill = SkillType::Attack;
    int damage = 0;
    int targetHpAfter = 0;
};

struct CombatResult {
    CombatWinner winner = CombatWinner::Player;
    Combatant player;
    Combatant monster;
    std::vector<CombatTurn> turns;
};

inline Combatant makeDefaultPlayer()
{
    return Combatant {"플레이어", 999, 9};
}

inline Combatant makeDefaultMonster(std::string name)
{
    if (name.empty()) {
        name = "몬스터";
    }
    return Combatant {std::move(name), 10, 1};
}

} // namespace textrpg::combat
