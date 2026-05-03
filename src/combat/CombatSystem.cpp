#include "combat/CombatSystem.hpp"

#include <algorithm>

namespace textrpg::combat {

    void CombatSystem::Action(CombatActor actor, Combatant& attacker, Combatant& target,
        SkillType type, CombatResult& result) const
    {
        int damage = 0;
        std::string desc;

        if (type == SkillType::Attack) 
        {
            damage = attacker.getAttack();
            int actualDamage = target.receiveDamage(damage);
            desc = attacker.getName() + "이(가) " + std::to_string(actualDamage) + "의 피해를 입혔습니다.";
        }

        // 전투 로그
        result.turns.push_back( CombatTurn{ actor,type,damage,target.getHp(),std::move(desc) } );
    }

    CombatResult CombatSystem::run(Combatant player, Combatant monster) const
    {
        CombatResult result;

        while (!player.isDead() && !monster.isDead()) 
        {
            // 플레이어 턴
            Action(CombatActor::Player, player, monster, SkillType::Attack, result);
            if (monster.isDead()) { break; }

            // 몬스터 턴
            Action(CombatActor::Monster, monster, player, SkillType::Attack, result);
        }
        
        //동시 사망시 플레이어 패배
        result.winner = (!player.isDead()) ? CombatWinner::Player : CombatWinner::Monster;

        result.player = std::move(player);
        result.monster = std::move(monster);
        return result;
    }

} // namespace textrpg::combat