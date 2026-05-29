#include "combat/CombatSystem.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <cstdlib>

namespace textrpg::combat {

    CombatResult CombatSystem::runRound(Combatant& player, Combatant& monster, int playerSkillIndex) const
    {
        CombatResult result;

        // 1. 몬스터 스킬은 무작위로 선택
        int mChoice = 0;
        if (!monster.getSkills().empty()) {
            mChoice = std::rand() % monster.getSkills().size();
        }

        // 2. 스피드 비교를 이용한 포켓몬 스타일 선공 결정
        bool playerGoesFirst = player.getSpeed() >= monster.getSpeed();

        Combatant* first = playerGoesFirst ? &player : &monster;
        Combatant* second = playerGoesFirst ? &monster : &player;

        CombatActor firstActor = playerGoesFirst ? CombatActor::Player : CombatActor::Monster;
        CombatActor secondActor = playerGoesFirst ? CombatActor::Monster : CombatActor::Player;

        int firstSkillIdx = playerGoesFirst ? playerSkillIndex : mChoice;
        int secondSkillIdx = playerGoesFirst ? mChoice : playerSkillIndex;

        // 3. 선공 행동 실행
        if (!first->getSkills().empty() && firstSkillIdx >= 0 && firstSkillIdx < first->getSkills().size()) {
            first->getSkills()[firstSkillIdx]->execute(firstActor, *first, *second, result);
        }

        // 4. 후공 행동 실행 (선공 공격에 의해 죽지 않았을 때만 실행)
        if (!second->isDead() && !second->getSkills().empty() && secondSkillIdx >= 0 && secondSkillIdx < second->getSkills().size()) {
            second->getSkills()[secondSkillIdx]->execute(secondActor, *second, *first, result);
        }

        // 5. 라운드 종료 시 상태이상 처리
        auto processStatuses = [&](CombatActor actor, Combatant& c) {
            if (c.isDead()) return;
            auto& statuses = c.getMutableStatuses();
            for (auto it = statuses.begin(); it != statuses.end(); ) {
                (*it)->onTurnEnd(actor, c, result);
                (*it)->tick();

                if ((*it)->isExpired()) {
                    it = statuses.erase(it);
                }
                else {
                    ++it;
                }
            }
            };

        processStatuses(CombatActor::Player, player);
        processStatuses(CombatActor::Monster, monster);

        // 6. 결과 정산 및 상태 백업
        result.player = player;
        result.monster = monster;

        if (player.isDead() || monster.isDead()) {
            result.finished = true;
            result.winner = (!player.isDead()) ? CombatWinner::Player : CombatWinner::Monster;
        }
        else {
            result.finished = false;
        }

        return result;
    }

} // namespace textrpg::combat