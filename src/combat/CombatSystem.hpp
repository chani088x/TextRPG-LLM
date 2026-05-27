#pragma once

#include "combat/CombatTypes.hpp"

#include <string>

namespace textrpg::combat
{

    class CombatSystem
    {
    public:
        // 전체 전투를 한 번에 처리한다 (기존). cout으로 직접 출력한다.
        CombatResult run(Combatant player, Combatant monster) const;

        // 1라운드(플레이어 공격 → 몬스터 공격)를 처리하고 결과를 반환한다.
        // CombatResolver::resolvePlayerAttack에서 사용한다.
        CombatResult runRound(Combatant player, Combatant monster) const;

        // 플레이어 고유 행동(d12 판정)으로 damage를 직접 지정해 1라운드를 처리한다.
        // CombatResolver::resolveCustomAction에서 사용한다.
        CombatResult runRound(
            Combatant player,
            Combatant monster,
            CombatActionType actionType,
            int damage,
            const std::string& actionDesc) const;

        // 몬스터 턴만 처리한다. 아이템 사용 후 몬스터 반격에 사용한다.
        // CombatResolver::resolveItemUse에서 사용한다.
        CombatResult runMonsterTurn(Combatant player, Combatant monster) const;
    };

} // namespace textrpg::combat