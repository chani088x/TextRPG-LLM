#pragma once

#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <algorithm>

namespace textrpg::combat {

    enum class SkillType {
        Attack,
        Defend,
        Draw //당장은 사용 x
    };

    enum class CombatActor {
        Player,
        Monster
    };

    enum class CombatWinner {
        Player,
        Monster
    };

    class Combatant;
    struct CombatResult;

    class Skill 
    {
    public:
        virtual ~Skill() = default;
        virtual std::unique_ptr<Skill> clone() const = 0;
        virtual void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const = 0;
    };

    class Combatant {
    private:
        std::string name;
        int hp;
        int maxHp;
        int attack;
        int defense;
        int speed;

        std::vector<std::unique_ptr<Skill>> skills;

    public:
        Combatant() : name("default"), hp(10), maxHp(100), attack(1), defense(0), speed(5) {}
        Combatant(std::string n, int h, int a, int d, int s = 5)
            : name(std::move(n)), hp(h), maxHp(h), attack(a), defense(d), speed(s) {
        }

        //복사 생성자
        Combatant(const Combatant& other)
            : name(other.name), hp(other.hp), maxHp(other.maxHp),
            attack(other.attack), defense(other.defense), speed(other.speed)
        {
            for (const auto& skill : other.skills) {
                skills.push_back(skill->clone());
            }
        }

        //복사 대입 연산자
        Combatant& operator=(const Combatant& other) {
            if (this != &other) {
                name = other.name; hp = other.hp; maxHp = other.maxHp;
                attack = other.attack; defense = other.defense; speed = other.speed;
                skills.clear();
                for (const auto& skill : other.skills) {
                    skills.push_back(skill->clone());
                }
            }
            return *this;
        }

        //이동 생성자 및 대입 연산자 (기본)
        Combatant(Combatant&&) noexcept = default;
        Combatant& operator=(Combatant&&) noexcept = default;

        int receiveDamage(int damage) {
            int resultDamage = 0;
            if (defense >= damage) {
                defense -= damage;
            }
            else {
                resultDamage = damage - defense;
                defense = 0;
                hp = std::max(0, hp - resultDamage);
            }
            return resultDamage;
        }

        void addDefense(int amount) { defense += amount; }
        void addSkill(std::unique_ptr<Skill> skill) { skills.push_back(std::move(skill)); }

        const std::string& getName() const { return name; }
        int getHp() const { return hp; }
        void setHp(int resultHp) { hp = resultHp; }
        int getAttack() const { return attack; }
        int getDefense() const { return defense; }
        int getSpeed() const { return speed; }
        bool isDead() const { return hp <= 0; }

        const std::vector<std::unique_ptr<Skill>>& getSkills() const { return skills; }
    };

    //전투 기록 및 결과 구조체
    struct CombatTurn {
        CombatActor actor = CombatActor::Player;
        SkillType type;
        int damage = 0;
        int targetHpAfter = 0;
        std::string description;
    };

    struct CombatResult {
        CombatWinner winner = CombatWinner::Player;
        Combatant player;
        Combatant monster;
        std::vector<CombatTurn> turns;
    };

    //스킬 구현체 (BasicAttack)
    class BasicAttack : public Skill {
    public:
        std::unique_ptr<Skill> clone() const override {
            return std::make_unique<BasicAttack>(*this);
        }

        void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const override {
            int damage = attacker.getAttack();
            int actualDamage = target.receiveDamage(damage);
            std::string desc = attacker.getName() + "이(가) " + std::to_string(actualDamage) + "의 피해를 입혔습니다.";

            result.turns.push_back(CombatTurn{ actor, SkillType::Attack, damage, target.getHp(), std::move(desc) });
        }
    };

    inline Combatant makeDefaultPlayer() {
        Combatant player{ "플레이어", 999, 9, 0, 10 };
        player.addSkill(std::make_unique<BasicAttack>());
        return player;
    }

    inline Combatant makeDefaultMonster(std::string name) {
        if (name.empty()) {
            name = "몬스터";
        }
        Combatant monster{ std::move(name), 10, 1, 0, 8 };
        monster.addSkill(std::make_unique<BasicAttack>());
        return monster;
    }

} // namespace textrpg::combat