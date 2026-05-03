#pragma once

#include <string>
#include <utility>
#include <vector>
#include <algorithm>

namespace textrpg::combat {

enum class SkillType {
    Attack,
    Defend,
    Draw //추후에 덱 시스템 완성시 사용
};

enum class CombatActor {
    Player,
    Monster
};

enum class CombatWinner {
    Player,
    Monster
};

//기존버전
//struct Combatant {
//    std::string name;
//    int hp = 1;
//    int attack = 1;
//    int defense = 0;
//};

//클래스 버전
class Combatant {
private:
    std::string name;
    int hp;
    int maxHp;
    int attack = 1;
    int defense = 0;
public:
    Combatant() : name("default"), hp(10), maxHp(100), attack(1), defense(0) {}
    Combatant(std::string n, int h, int a, int d) : name(std::move(n)), hp(h), maxHp(h), attack(a), defense(d) {}

    int receiveDamage(int damage)
    {
        int resultDamage = 0;
        if (defense >= damage) 
        {
            defense -= damage;
        }
        else 
        {
            resultDamage = damage - defense;
            defense = 0;
            hp = std::max(0, hp - resultDamage);
        }
        return resultDamage;
    }

    void addDefense(int amount) { defense += amount; }

    const std::string& getName() const { return name; }
    int getHp() const { return hp; }
    void setHp(int resultHp)
    {
        hp = resultHp;
    }
    int getAttack() const { return attack; }
    int getDefense() const { return defense; }
    bool isDead() const { return hp <= 0; }
};

struct CombatTurn {
    CombatActor actor = CombatActor::Player;
    SkillType type;
    int damage = 0;
    int targetHpAfter = 0;
    std::string description; //행동 설명
};

struct CombatResult {
    CombatWinner winner = CombatWinner::Player;
    Combatant player;
    Combatant monster;
    std::vector<CombatTurn> turns;
};

inline Combatant makeDefaultPlayer()
{
    return Combatant {"플레이어", 999, 9, 0};
}

inline Combatant makeDefaultMonster(std::string name)
{
    if (name.empty()) {
        name = "몬스터";
    }
    return Combatant {std::move(name), 10, 1, 0};
}

} // namespace textrpg::combat
