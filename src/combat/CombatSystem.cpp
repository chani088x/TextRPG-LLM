#include "combat/CombatSystem.hpp"

#include <algorithm>

namespace textrpg::combat {
namespace {

void attack(CombatActor actor, Combatant& attacker, Combatant& target, CombatResult& result)
{
    const auto damage = std::max(0, attacker.attack);
    target.hp = std::max(0, target.hp - damage);
    result.turns.push_back(CombatTurn {actor, SkillType::Attack, damage, target.hp});
}

} // namespace

CombatResult CombatSystem::run(Combatant player, Combatant monster) const
{
    CombatResult result;

    player.hp = std::max(0, player.hp);
    player.attack = std::max(0, player.attack);
    monster.hp = std::max(0, monster.hp);
    monster.attack = std::max(0, monster.attack);

    while (player.hp > 0 && monster.hp > 0) {
        attack(CombatActor::Player, player, monster, result);
        if (monster.hp <= 0) {
            break;
        }

        attack(CombatActor::Monster, monster, player, result);
    }

    result.winner = player.hp > 0 ? CombatWinner::Player : CombatWinner::Monster;
    result.player = std::move(player);
    result.monster = std::move(monster);
    return result;
}

} // namespace textrpg::combat
