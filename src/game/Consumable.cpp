#include "game/Consumable.hpp"

#include <sstream>

namespace textrpg::game {

Consumable::Consumable(std::string name, std::string description, int value,
                       int hpRestore, int quantity)
    : Item(std::move(name), std::move(description), value)
    , hpRestore_(hpRestore)
    , quantity_(quantity)
{
}

std::string Consumable::type() const
{
    return llm::ids::item::Consumable;
}

std::string Consumable::summary() const
{
    std::ostringstream oss;
    oss << name_ << " [소모품]";
    if (hpRestore_ > 0) {
        oss << " HP+" << hpRestore_;
    }
    if (quantity_ > 1) {
        oss << " x" << quantity_;
    }
    return oss.str();
}

bool Consumable::consume()
{
    if (quantity_ <= 0) {
        return false;
    }
    --quantity_;
    return quantity_ > 0;
}

} // namespace textrpg::game
