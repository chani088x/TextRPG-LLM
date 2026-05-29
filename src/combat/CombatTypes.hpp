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
        Draw // 당장은 사용 x
    };

    enum class ElementType {
        Normal, // 노말
        Fire,   // 불
        Water,  // 물
        Grass,  // 풀
        Dark,   // 어둠
        Light   // 빛
    };

    inline float getElementMultiplier(ElementType attackType, ElementType defendType) {
        // 1. 공격에 유리한 경우 (2배 데미지)
        if (attackType == ElementType::Fire && defendType == ElementType::Grass) return 2.0f;
        if (attackType == ElementType::Water && defendType == ElementType::Fire) return 2.0f;
        if (attackType == ElementType::Grass && defendType == ElementType::Water) return 2.0f;
        if (attackType == ElementType::Dark && defendType == ElementType::Light) return 2.0f;
        if (attackType == ElementType::Light && defendType == ElementType::Dark) return 2.0f;

        // 2. 공격에 불리한 경우 (절반 데미지)
        if (attackType == ElementType::Fire && defendType == ElementType::Water) return 0.5f;
        if (attackType == ElementType::Water && defendType == ElementType::Grass) return 0.5f;
        if (attackType == ElementType::Grass && defendType == ElementType::Fire) return 0.5f;

        // 3. 그 외의 경우 (상성 없음, 1배 데미지)
        return 1.0f;
    }

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

    class Skill {
    public:
        virtual ~Skill() = default;
        virtual std::string getName() const = 0;

        // 기본은 노말 타입
        virtual ElementType getElement() const { return ElementType::Normal; }

        virtual std::unique_ptr<Skill> clone() const = 0;
        virtual void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const = 0;
    };

    // 상태이상
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
        ElementType element; // 방어 속성

        std::vector<std::unique_ptr<Skill>> skills;
        std::vector<std::unique_ptr<StatusEffect>> statuses;

    public:
        Combatant() : name("default"), hp(10), maxHp(100), attack(1), defense(0), speed(5), element(ElementType::Normal) {}
        Combatant(std::string n, int h, int a, int d, int s = 5, ElementType elem = ElementType::Normal)
            : name(std::move(n)), hp(h), maxHp(h), attack(a), defense(d), speed(s), element(elem) {
        }

        // 복사 생성자
        Combatant(const Combatant& other)
            : name(other.name), hp(other.hp), maxHp(other.maxHp),
            attack(other.attack), defense(other.defense), speed(other.speed), element(other.element)
        {
            for (const auto& skill : other.skills) skills.push_back(skill->clone());
            for (const auto& status : other.statuses) statuses.push_back(status->clone());
        }

        // 복사 대입 연산자
        Combatant& operator=(const Combatant& other) {
            if (this != &other) {
                name = other.name; hp = other.hp; maxHp = other.maxHp;
                attack = other.attack; defense = other.defense; speed = other.speed; element = other.element;
                skills.clear(); statuses.clear();
                for (const auto& skill : other.skills) skills.push_back(skill->clone());
                for (const auto& status : other.statuses) statuses.push_back(status->clone());
            }
            return *this;
        }

        // 이동 생성자 및 대입 연산자 (기본)
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
        // 상태이상 확인
        std::vector<std::unique_ptr<StatusEffect>>& getMutableStatuses() { return statuses; }

        ElementType getElement() const { return element; }

        const std::string& getName() const { return name; }
        int getHp() const { return hp; }
        void setHp(int resultHp) { hp = resultHp; }
        int getAttack() const { return attack; }
        int getDefense() const { return defense; }
        int getSpeed() const { return speed; }
        bool isDead() const { return hp <= 0; }

        const std::vector<std::unique_ptr<Skill>>& getSkills() const { return skills; }
    };

    // 전투 기록 및 결과 구조체
    struct CombatTurn {
        CombatActor actor = CombatActor::Player;
        SkillType type;
        int damage = 0;
        int targetHpAfter = 0;
        std::string description;
    };

    struct CombatResult {
        bool finished = false;
        CombatWinner winner = CombatWinner::Player;
        Combatant player;
        Combatant monster;
        std::vector<CombatTurn> turns;
    };

    // 스킬 구현체 (BasicAttack)
    class BasicAttack : public Skill {
    public:
        std::string getName() const override { return "기본 공격"; }

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
        std::string getName() const override { return "급소 찌르기"; }

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
            int damage = std::max(1, attacker.getAttack() / 2);
            int actualDamage = target.receiveDamage(damage);

            target.addStatus(std::make_unique<PoisonStatus>(3, 5));

            std::string desc = attacker.getName() + "의 [" + getName() + "]! " +
                std::to_string(actualDamage) + "의 피해를 입히고 독에 감염시켰습니다!";

            result.turns.push_back(CombatTurn{ actor, SkillType::Attack, damage, target.getHp(), std::move(desc) });
        }
    };

    class Fireball : public Skill {
    public:
        std::string getName() const override { return "화염구"; }

        ElementType getElement() const override { return ElementType::Fire; }

        std::unique_ptr<Skill> clone() const override {
            return std::make_unique<Fireball>(*this);
        }

        void execute(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result) const override {
            float multiplier = getElementMultiplier(this->getElement(), target.getElement());

            int baseDamage = attacker.getAttack();
            int finalDamage = static_cast<int>(baseDamage * multiplier);
            int actualDamage = target.receiveDamage(finalDamage);

            std::string effText = "";
            if (multiplier > 1.0f) effText = " (효과가 굉장했다!)";
            else if (multiplier < 1.0f) effText = " (효과가 별로인 것 같다...)";

            std::string desc = attacker.getName() + "의 [" + getName() + "]! " +
                std::to_string(actualDamage) + "의 피해를 입혔습니다!" + effText;

            result.turns.push_back(CombatTurn{ actor, SkillType::Attack, actualDamage, target.getHp(), std::move(desc) });
        }
    };

    inline Combatant makeDefaultPlayer() {
        Combatant player{ "플레이어", 999, 9, 0, 10 };
        player.addSkill(std::make_unique<BasicAttack>());
        player.addSkill(std::make_unique<CriticalHit>());
        player.addSkill(std::make_unique<PoisonSkill>());
        player.addSkill(std::make_unique<Fireball>());
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