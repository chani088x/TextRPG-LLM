#include "combat/CombatResolver.hpp"

#include "combat/CombatSystem.hpp"
#include "game/Inventory.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <vector>

namespace textrpg::combat {
namespace {

void pushLimited(std::vector<std::string>& values, const std::string& value, std::size_t maxCount)
{
    if (value.empty()) {
        return;
    }

    values.push_back(value);
    while (values.size() > maxCount) {
        values.erase(values.begin());
    }
}

Combatant playerFromState(const llm::GameState& state)
{
    return Combatant {
        "플레이어",
        state.player.hp,
        std::max(1, state.player.attack),
        std::max(0, state.player.defense),
    };
}

Combatant monsterFromState(const llm::Monster& monster)
{
    return Combatant {
        monster.name.empty() ? "몬스터" : monster.name,
        std::max(1, monster.hp),
        std::max(1, monster.attack),
        std::max(0, monster.defense),
    };
}

std::string formatCombatResult(const CombatResult& result)
{
    std::ostringstream out;
    out << "공방이 이어졌다.\n";
    for (const auto& turn : result.turns) {
        const auto actor = turn.actor == CombatActor::Player ? "플레이어" : result.monster.getName();
        const auto target = turn.actor == CombatActor::Player ? result.monster.getName() : "플레이어";
        out << actor << " -> " << target
            << " " << turn.damage << " 피해"
            << " | 남은 HP " << turn.targetHpAfter << '\n';
    }

    if (result.finished) {
        out << (result.winner == CombatWinner::Player
                ? "적이 쓰러졌다."
                : "플레이어가 쓰러졌다.")
            << '\n';
    } else {
        out << "전투가 계속된다. "
            << result.monster.getName() << " HP " << result.monster.getHp()
            << ", 플레이어 HP " << result.player.getHp() << '\n';
    }
    return out.str();
}

std::string summarizeCombatState(const CombatResult& result)
{
    if (result.finished) {
        if (result.winner == CombatWinner::Player) {
            return "플레이어가 " + result.monster.getName() + "를 쓰러뜨렸다.";
        }
        return result.monster.getName() + "에게 쓰러졌다.";
    }

    std::ostringstream out;
    out << result.monster.getName() << "와 전투 중이다. 적 HP "
        << result.monster.getHp() << ", 플레이어 HP " << result.player.getHp() << '.';
    return out.str();
}

void syncCombatResultToState(
    llm::GameState& state,
    std::optional<llm::Monster>& activeMonster,
    const CombatResult& result)
{
    state.player.hp = result.player.getHp();

    if (result.finished) {
        activeMonster.reset();
        return;
    }

    if (activeMonster.has_value()) {
        activeMonster->hp = result.monster.getHp();
        activeMonster->defense = result.monster.getDefense();
    }
}

int customDamageForOutcome(const llm::GameState& state, const std::string& outcome)
{
    if (outcome == llm::ids::dice::Jackpot) {
        return std::max(8, state.player.attack * 2);
    }
    if (outcome == llm::ids::dice::Success) {
        return std::max(4, state.player.attack);
    }
    return 1;
}

std::optional<llm::Item> findUsableConsumable(game::Inventory& inventory)
{
    auto consumables = inventory.getConsumables();
    if (consumables.empty()) {
        return std::nullopt;
    }

    const auto* item = consumables.back();
    return llm::Item {
        item->name(),
        item->type(),
        item->description(),
        item->value(),
    };
}

int consumableHealAmount(game::Inventory& inventory, const std::string& name)
{
    for (const auto* item : inventory.getConsumables()) {
        if (item->name() == name) {
            return std::max(1, item->hpRestore());
        }
    }
    return 1;
}

} // namespace

bool CombatResolver::isActive() const
{
    return activeMonster_.has_value();
}

void CombatResolver::updateFromEvent(const llm::GameEvent& event)
{
    if (event.eventType == llm::ids::event::Combat && event.monster.has_value()) {
        activeMonster_ = event.monster;
        return;
    }

    activeMonster_.reset();
}

CombatResolution CombatResolver::resolvePlayerAttack(llm::GameState& state)
{
    if (!activeMonster_.has_value()) {
        return {
            "",
            "플레이어는 공격 자세를 취했지만 눈앞에 확실한 적은 없었다.",
        };
    }

    const CombatSystem combatSystem;
    const auto result = combatSystem.runRound(playerFromState(state), monsterFromState(activeMonster_.value()));

    syncCombatResultToState(state, activeMonster_, result);
    const auto summary = summarizeCombatState(result);
    pushLimited(state.memory.recentEvents, "전투: " + summary, 5);

    return {
        formatCombatResult(result),
        summary,
    };
}

CombatResolution CombatResolver::resolveCustomAction(
    llm::GameState& state,
    const std::string& actionText,
    const std::string& diceOutcome,
    int diceValue)
{
    if (!activeMonster_.has_value()) {
        return {
            "",
            "플레이어는 " + actionText + "을(를) 시도했지만 눈앞에 확실한 적은 없었다.",
        };
    }

    const CombatSystem combatSystem;
    const int damage = customDamageForOutcome(state, diceOutcome);
    std::ostringstream desc;
    desc << "플레이어의 고유 행동 [" << actionText << "]이(d12 " << diceValue << ")";
    const auto result = combatSystem.runRound(
        playerFromState(state),
        monsterFromState(activeMonster_.value()),
        CombatActionType::Custom,
        damage,
        desc.str());

    syncCombatResultToState(state, activeMonster_, result);
    const auto summary = summarizeCombatState(result);
    pushLimited(state.memory.recentEvents, "전투: " + summary, 5);

    return {
        formatCombatResult(result),
        summary,
    };
}

CombatResolution CombatResolver::resolveItemUse(llm::GameState& state)
{
    auto inventory = game::Inventory::fromLLMItems(state.player.inventory);
    const auto item = findUsableConsumable(inventory);
    if (!item.has_value()) {
        return {
            "",
            "플레이어는 [아이템]을 확인했지만 사용할 소모품이 없다.",
        };
    }

    if (state.player.hp >= state.player.maxHp) {
        return {
            "",
            "플레이어는 " + item->name + "을 확인했지만 HP가 이미 가득 차 있어 아껴 두었다.",
        };
    }

    const int beforeHp = state.player.hp;
    const int healAmount = consumableHealAmount(inventory, item->name);
    state.player.hp = llm::clampInt(state.player.hp + healAmount, 1, state.player.maxHp);
    inventory.removeItem(item->name);
    state.player.inventory = inventory.toLLMItems();

    std::ostringstream out;
    out << "플레이어는 [아이템] " << item->name << "을 사용했다. HP "
        << beforeHp << " -> " << state.player.hp << '.';
    pushLimited(state.memory.recentEvents, "아이템 사용: " + item->name, 5);

    if (activeMonster_.has_value()) {
        const CombatSystem combatSystem;
        const auto result = combatSystem.runMonsterTurn(playerFromState(state), monsterFromState(activeMonster_.value()));
        syncCombatResultToState(state, activeMonster_, result);
        out << "\n" << formatCombatResult(result);
        pushLimited(state.memory.recentEvents, "전투: " + summarizeCombatState(result), 5);
    }

    return {
        "",
        out.str(),
    };
}

} // namespace textrpg::combat
