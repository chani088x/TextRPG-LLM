#include "game/Item.hpp"

#include "game/Consumable.hpp"
#include "game/Equipment.hpp"
#include "game/QuestItem.hpp"

namespace textrpg::game {

Item::Item(std::string name, std::string description, int value)
    : name_(std::move(name))
    , description_(std::move(description))
    , value_(value)
{
}

std::unique_ptr<Item> Item::fromLLMItem(const llm::Item& llmItem)
{
    switch (llmItem.type) {
    case llm::ItemType::Weapon:
        return std::make_unique<Equipment>(
            llmItem.name, llmItem.description, llmItem.value,
            EquipSlot::Weapon,
            /*attackBonus=*/ llmItem.value / 10,
            /*defenseBonus=*/ 0);

    case llm::ItemType::Armor:
        return std::make_unique<Equipment>(
            llmItem.name, llmItem.description, llmItem.value,
            EquipSlot::Armor,
            /*attackBonus=*/ 0,
            /*defenseBonus=*/ llmItem.value / 10);

    case llm::ItemType::Consumable:
        return std::make_unique<Consumable>(
            llmItem.name, llmItem.description, llmItem.value,
            /*hpRestore=*/ llmItem.value * 2);

    case llm::ItemType::QuestItem:
    default:
        return std::make_unique<QuestItem>(
            llmItem.name, llmItem.description, llmItem.value);
    }
}

} // namespace textrpg::game
