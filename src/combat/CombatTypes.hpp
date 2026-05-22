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
        virtual std::string getName() const = 0;
        virtual std::unique_ptr<Skill> clone() const = 0;
        virtual void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const = 0;
    };

    //상태이상
    class StatusEffect {
    protected:
        std::string name;
        int duration; // 남은 턴 수
    public:
        StatusEffect(std::string n, int d) : name(std::move(n)), duration(d) {}
        virtual ~StatusEffect() = default;

        virtual std::unique_ptr<StatusEffect> clone() const = 0;

        const std::string& getName() const { return name; }
        bool isExpired() const { return duration <= 0; }
        void tick() { if (duration > 0) duration--; } // 턴 감소

        // 턴 종료 시 발동할 효과
        virtual void onTurnEnd(CombatActor actor, Combatant& target, CombatResult& result) = 0;
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
        std::vector<std::unique_ptr<StatusEffect>> statuses;

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
            for (const auto& skill : other.skills) skills.push_back(skill->clone());
            for (const auto& status : other.statuses) statuses.push_back(status->clone());
        }

        //복사 대입 연산자
        Combatant& operator=(const Combatant& other) {
            if (this != &other) {
                name = other.name; hp = other.hp; maxHp = other.maxHp;
                attack = other.attack; defense = other.defense; speed = other.speed;
                skills.clear(); statuses.clear();
                for (const auto& skill : other.skills) skills.push_back(skill->clone());
                for (const auto& status : other.statuses) statuses.push_back(status->clone());
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

        void addStatus(std::unique_ptr<StatusEffect> status) { statuses.push_back(std::move(status)); }
        //상태이상 확인
        std::vector<std::unique_ptr<StatusEffect>>& getMutableStatuses() { return statuses; }

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
        std::string getName() const override { return "기본 공격"; } // 이름 추가

        std::unique_ptr<Skill> clone() const override {
            return std::make_unique<BasicAttack>(*this);
        }

        void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const override {
            int damage = attacker.getAttack();
            int actualDamage = target.receiveDamage(damage);
            std::string desc = attacker.getName() + "이(가) [" + getName() + "]! " + std::to_string(actualDamage) + "의 피해를 입혔습니다.";

            result.turns.push_back(CombatTurn{ actor, SkillType::Attack, damage, target.getHp(), std::move(desc) });
        }
    };

    class CriticalHit : public Skill {
    public:
        std::string getName() const override { return "급소 찌르기"; } // 이름 추가

        std::unique_ptr<Skill> clone() const override {
            return std::make_unique<CriticalHit>(*this);
        }

        void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const override {
            int damage = attacker.getAttack() * 2;
            int actualDamage = target.receiveDamage(damage);
            std::string desc = attacker.getName() + "의 [" + getName() + "]!! " + std::to_string(actualDamage) + "의 치명적인 피해를 입혔습니다.";

            result.turns.push_back(CombatTurn{ actor, SkillType::Attack, damage, target.getHp(), std::move(desc) });
        }
    };

    
    class PoisonStatus : public StatusEffect {
    private:
        int poisonDamage; // 매 턴 입을 데미지
    public:
        PoisonStatus(int duration, int damage) : StatusEffect("독", duration), poisonDamage(damage) {}

        std::unique_ptr<StatusEffect> clone() const override {
            return std::make_unique<PoisonStatus>(*this);
        }

        void onTurnEnd(CombatActor actor, Combatant& target, CombatResult& result) override {
            int actualDamage = target.receiveDamage(poisonDamage);

            // duration - 1을 하는 이유는 이번 턴 처리가 끝나면 tick()으로 줄어들 것이기 때문
            std::string desc = "  [상태이상] " + target.getName() + "이(가) 독에 의해 " +
                std::to_string(actualDamage) + "의 피해를 입었습니다! (남은 턴: " + std::to_string(duration - 1) + ")";

            result.turns.push_back(CombatTurn{ actor, SkillType::Attack, actualDamage, target.getHp(), std::move(desc) });
        }
    };

    class PoisonSkill : public Skill {
    public:
        std::string getName() const override { return "맹독 찌르기"; }

        std::unique_ptr<Skill> clone() const override {
            return std::make_unique<PoisonSkill>(*this);
        }

        void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const override {
            // 독침 자체의 데미지는 약하게 설정 (공격력의 절반)
            int damage = std::max(1, attacker.getAttack() / 2);
            int actualDamage = target.receiveDamage(damage);

            // 타겟에게 3턴 동안 매 턴 5의 피해를 주는 독 상태 이상 부여
            target.addStatus(std::make_unique<PoisonStatus>(3, 5));

            std::string desc = attacker.getName() + "의 [" + getName() + "]! " +
                std::to_string(actualDamage) + "의 피해를 입히고 독에 감염시켰습니다!";

            result.turns.push_back(CombatTurn{ actor, SkillType::Attack, damage, target.getHp(), std::move(desc) });
        }
    };

    inline Combatant makeDefaultPlayer() {
        Combatant player{ "플레이어", 999, 9, 0, 10 };
        player.addSkill(std::make_unique<BasicAttack>());
        player.addSkill(std::make_unique<CriticalHit>());
        player.addSkill(std::make_unique<PoisonSkill>());
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