#include "game/DangerSystem.hpp"

#include <random>

namespace textrpg::game {
namespace {

int weightedDangerIncrease()
{
    constexpr int maxIncrease = 10;
    constexpr int totalWeight = 55;

    static std::mt19937 rng(std::random_device {}());
    std::uniform_int_distribution<int> distribution(0, totalWeight - 1);
    int roll = distribution(rng);
    for (int increase = 1; increase <= maxIncrease; ++increase) {
        const int weight = maxIncrease + 1 - increase;
        if (roll < weight) {
            return increase;
        }
        roll -= weight;
    }
    return maxIncrease;
}

} // namespace

DangerAdvance DangerSystem::advanceTurn(llm::GameState& state) const
{
    auto& danger = state.records.danger;
    if (danger.threshold <= 0) {
        danger.threshold = 10;
    }

    DangerAdvance result;
    result.previous = danger.level;
    result.increase = weightedDangerIncrease();
    result.current = danger.level + result.increase;
    result.threshold = danger.threshold;
    result.combatTriggered = result.current >= danger.threshold;

    danger.level = result.current;
    danger.lastIncrease = result.increase;
    return result;
}

void DangerSystem::resetAfterCombat(llm::GameState& state) const
{
    state.records.danger.level = 0;
    state.records.danger.lastIncrease = 0;
    if (state.records.danger.threshold <= 0) {
        state.records.danger.threshold = 10;
    }
}

} // namespace textrpg::game
