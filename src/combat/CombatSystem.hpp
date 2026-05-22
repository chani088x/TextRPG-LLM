#pragma once

#include "combat/CombatTypes.hpp"

namespace textrpg::combat {

class CombatSystem {
public:
    CombatResult run(Combatant player, Combatant monster) const;
    CombatResult runRound(
        Combatant player,
        Combatant monster,
        CombatActionType playerAction = CombatActionType::Attack,
        int customDamage = 0,
        const std::string& customDescription = {}) const;
    CombatResult runMonsterTurn(Combatant player, Combatant monster) const;
private:
    void applyAction(
        CombatActor actor,
        Combatant& attacker,
        Combatant& target,
        CombatActionType type,
        CombatResult& result,
        int customDamage = 0,
        const std::string& customDescription = {}) const;
};

} // namespace textrpg::combat
