#include "combat/CombatSystem.hpp"
#include <iostream>
#include <string>

namespace textrpg::combat {

    CombatResult CombatSystem::run(Combatant player, Combatant monster) const
    {
        CombatResult result;

        while (!player.isDead() && !monster.isDead())
        {
            std::cout << "\n========================================\n";
            std::cout << "[전투 중] " << player.getName() << " HP: " << player.getHp()
                << "  VS  " << monster.getName() << " HP: " << monster.getHp() << "\n";
            std::cout << "========================================\n";

            // 1. 플레이어 스킬 선택 메뉴
            std::cout << "사용할 스킬을 선택하세요:\n";
            const auto& playerSkills = player.getSkills();
            for (size_t i = 0; i < playerSkills.size(); ++i) {
                std::cout << "  " << i + 1 << ". " << playerSkills[i]->getName() << "\n";
            }
            std::cout << "선택: ";

            // 플레이어 입력 받기
            std::string input;
            std::getline(std::cin, input);

            int pChoice = 0;
            try { pChoice = std::stoi(input) - 1; }
            catch (...) { pChoice = 0; } // 엔터만 치거나 이상한 글자 쓰면 1번 스킬로 강제

            if (pChoice < 0 || pChoice >= playerSkills.size()) {
                pChoice = 0;
            }

            // 2. 몬스터 스킬은 무작위로 선택
            int mChoice = 0;
            if (!monster.getSkills().empty()) {
                mChoice = std::rand() % monster.getSkills().size();
            }

            // 3. 스피드 비교 (포켓몬 스타일 선공 판정)
            bool playerGoesFirst = player.getSpeed() >= monster.getSpeed();

            Combatant* first = playerGoesFirst ? &player : &monster;
            Combatant* second = playerGoesFirst ? &monster : &player;

            CombatActor firstActor = playerGoesFirst ? CombatActor::Player : CombatActor::Monster;
            CombatActor secondActor = playerGoesFirst ? CombatActor::Monster : CombatActor::Player;

            int firstSkillIdx = playerGoesFirst ? pChoice : mChoice;
            int secondSkillIdx = playerGoesFirst ? mChoice : pChoice;

            std::cout << "\n";

            // 4. 선공의 행동 실행 및 즉시 출력
            if (!first->getSkills().empty()) {
                first->getSkills()[firstSkillIdx]->execute(firstActor, *first, *second, result);
                std::cout << result.turns.back().description << "\n"; // 방금 행동한 텍스트 출력
            }

            // 선공의 공격으로 타겟이 쓰러졌다면 루프 탈출
            if (second->isDead()) { break; }

            // 5. 후공의 행동 실행 및 즉시 출력
            if (!second->getSkills().empty()) {
                second->getSkills()[secondSkillIdx]->execute(secondActor, *second, *first, result);
                std::cout << result.turns.back().description << "\n";
            }
            // 6.상태 이상 처리
            auto processStatuses = [&](CombatActor actor, Combatant& c) 
                {
                if (c.isDead()) return;
                auto& statuses = c.getMutableStatuses();
                for (auto it = statuses.begin(); it != statuses.end(); ) 
                {
                    // 효과 발동 및 출력
                    (*it)->onTurnEnd(actor, c, result);
                    std::cout << result.turns.back().description << "\n";

                    // 턴 수(duration) 감소
                    (*it)->tick();

                    // 수명이 다했으면 벡터에서 제거, 아니면 다음 상태이상으로 넘어감
                    if ((*it)->isExpired()) 
                    {
                        it = statuses.erase(it);
                    }
                    else 
                    {
                        ++it;
                    }
                }
                };

            // 플레이어와 몬스터의 상태 이상을 각각 처리
            processStatuses(CombatActor::Player, player);
            processStatuses(CombatActor::Monster, monster);

            std::cout << "----------------------------------------\n";
        }

        // 전투 종료 후 최종 정산
        result.winner = (!player.isDead()) ? CombatWinner::Player : CombatWinner::Monster;
        result.player = std::move(player);
        result.monster = std::move(monster);

        return result;
    }

} // namespace textrpg::combat