#include "combat/CombatSystem.hpp"

#include <algorithm>

namespace textrpg::combat {

void CombatSystem::applyAction(
    CombatActor actor,
    Combatant& attacker,
    Combatant& target,
    CombatActionType type,
    CombatResult& result,
    int customDamage,
    const std::string& customDescription) const
{
    int damage = 0;
    std::string desc;

    if (type == CombatActionType::Attack) {
        damage = attacker.getAttack();
        const int actualDamage = target.receiveDamage(damage);
        damage = actualDamage;
        desc = attacker.getName() + "이(가) " + std::to_string(actualDamage) + "의 피해를 입혔습니다.";
    } else if (type == CombatActionType::Custom) {
        damage = std::max(1, customDamage);
        const int actualDamage = target.receiveDamage(damage);
        damage = actualDamage;
        desc = customDescription.empty()
            ? attacker.getName() + "의 고유 행동이 " + std::to_string(actualDamage) + "의 피해를 입혔습니다."
            : customDescription + " " + std::to_string(actualDamage) + "의 피해를 입혔습니다.";
    }

    result.turns.push_back(CombatTurn {actor, type, damage, target.getHp(), std::move(desc)});
}

CombatResult CombatSystem::runRound(
    Combatant player,
    Combatant monster,
    CombatActionType playerAction,
    int customDamage,
    const std::string& customDescription) const
{
    CombatResult result;

    if (!player.isDead() && !monster.isDead()) {
        applyAction(CombatActor::Player, player, monster, playerAction, result, customDamage, customDescription);
    }

    if (monster.isDead()) {
        result.finished = true;
        result.winner = CombatWinner::Player;
    } else if (!player.isDead()) {
        applyAction(CombatActor::Monster, monster, player, CombatActionType::Attack, result);
        if (player.isDead()) {
            result.finished = true;
            result.winner = CombatWinner::Monster;
        }
    }

    result.player = std::move(player);
    result.monster = std::move(monster);
    return result;
}

CombatResult CombatSystem::runMonsterTurn(Combatant player, Combatant monster) const
{
    CombatResult result;

    if (!player.isDead() && !monster.isDead()) {
        applyAction(CombatActor::Monster, monster, player, CombatActionType::Attack, result);
        if (player.isDead()) {
            result.finished = true;
            result.winner = CombatWinner::Monster;
        }
    }

    result.player = std::move(player);
    result.monster = std::move(monster);
    return result;
}

CombatResult CombatSystem::run(Combatant player, Combatant monster) const
{
    CombatResult result;

    while (!player.isDead() && !monster.isDead()) {
        auto round = runRound(player, monster);
        result.turns.insert(result.turns.end(), round.turns.begin(), round.turns.end());
        player = std::move(round.player);
        monster = std::move(round.monster);
    }

    result.finished = true;
    result.winner = (!player.isDead()) ? CombatWinner::Player : CombatWinner::Monster;
    result.player = std::move(player);
    result.monster = std::move(monster);
    return result;
}

} // namespace textrpg::combat
