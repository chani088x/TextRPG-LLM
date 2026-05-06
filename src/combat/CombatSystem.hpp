#pragma once

#include "combat/CombatTypes.hpp"

namespace textrpg::combat {

class CombatSystem {
public:
    CombatResult run(Combatant player, Combatant monster) const;
};

} // namespace textrpg::combat
