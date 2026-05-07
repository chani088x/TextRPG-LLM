#include "game/Equipment.hpp"

#include <sstream>

namespace textrpg::game {

Equipment::Equipment(std::string name, std::string description, int value,
                     EquipSlot slot, int attackBonus, int defenseBonus)
    : Item(std::move(name), std::move(description), value)
    , slot_(slot)
    , attackBonus_(attackBonus)
    , defenseBonus_(defenseBonus)
{
}

std::string Equipment::type() const
{
    return slot_ == EquipSlot::Weapon ? llm::ids::item::Weapon : llm::ids::item::Armor;
}

std::string Equipment::summary() const
{
    std::ostringstream oss;
    oss << name_ << " [" << (slot_ == EquipSlot::Weapon ? "무기" : "방어구") << "]";
    if (attackBonus_ > 0) {
        oss << " ATK+" << attackBonus_;
    }
    if (defenseBonus_ > 0) {
        oss << " DEF+" << defenseBonus_;
    }
    if (equipped_) {
        oss << " (장착 중)";
    }
    return oss.str();
}

void Equipment::equip()
{
    equipped_ = true;
}

void Equipment::unequip()
{
    equipped_ = false;
}

} // namespace textrpg::game
