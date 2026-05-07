#pragma once

#include "game/Item.hpp"

namespace textrpg::game {

enum class EquipSlot { Weapon, Armor };

class Equipment : public Item {
public:
    Equipment(std::string name, std::string description, int value,
              EquipSlot slot, int attackBonus, int defenseBonus);

    std::string   type()    const override;
    std::string   summary() const override;

    EquipSlot slot()         const { return slot_; }
    int       attackBonus()  const { return attackBonus_; }
    int       defenseBonus() const { return defenseBonus_; }
    bool      isEquipped()   const { return equipped_; }

    void equip();
    void unequip();

private:
    EquipSlot slot_;
    int       attackBonus_;
    int       defenseBonus_;
    bool      equipped_ = false;
};

} // namespace textrpg::game
