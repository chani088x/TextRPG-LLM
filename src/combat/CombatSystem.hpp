#pragma once

#include "combat/CombatTypes.hpp"

namespace textrpg::combat {

class CombatSystem {
public:
    CombatResult run(Combatant player, Combatant monster) const;
private:
    void Action(CombatActor actor, Combatant& attacker, Combatant& target, SkillType type, CombatResult& result) const;
};

} // namespace textrpg::combat
