#include "combat/CombatSystem.hpp"

namespace textrpg::combat {

    CombatResult CombatSystem::run(Combatant player, Combatant monster) const
    {
        CombatResult result;

        // 한쪽이 쓰러질 때까지 전투
        while (!player.isDead() && !monster.isDead())
        {
            //선공 판정
            bool playerGoesFirst = player.getSpeed() >= monster.getSpeed();

            Combatant* first = playerGoesFirst ? &player : &monster;
            Combatant* second = playerGoesFirst ? &monster : &player;

            CombatActor firstActor = playerGoesFirst ? CombatActor::Player : CombatActor::Monster;
            CombatActor secondActor = playerGoesFirst ? CombatActor::Monster : CombatActor::Player;

            // 2. 선공의 턴
            if (!first->getSkills().empty()) 
            {
                first->getSkills()[0]->execute(firstActor, *first, *second, result);
            }
           
            if (second->isDead()) { break; }

            // 4. 후공의 턴
            if (!second->getSkills().empty()) 
            {
                second->getSkills()[0]->execute(secondActor, *second, *first, result);
            }
        }

        // 전투 결과 정산
        result.winner = (!player.isDead()) ? CombatWinner::Player : CombatWinner::Monster;
        result.player = std::move(player);
        result.monster = std::move(monster);

        return result;
    }

} // namespace textrpg::combat