#pragma once

#include "combat/CombatTypes.hpp"

namespace textrpg::combat
{
    class CombatSystem
    {
    public:
        // 콘솔 입력을 직접 받던 run 함수 대신, 유저가 UI에서 선택한 스킬 인덱스를 받아 1라운드를 해결함
        CombatResult runRound(Combatant& player, Combatant& monster, int playerSkillIndex) const;
    };

} // namespace textrpg::combat