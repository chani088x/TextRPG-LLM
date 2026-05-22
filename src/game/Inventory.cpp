#include "game/Inventory.hpp"

#include <algorithm>
#include <utility>

namespace textrpg::game {

Inventory Inventory::fromLLMItems(const std::vector<llm::Item>& items)
{
    Inventory inventory;
    for (const auto& item : items) {
        inventory.addLLMItem(item);
    }
    return inventory;
}

bool Inventory::addItem(std::unique_ptr<Item> item)
{
    if (!item) {
        return false;
    }

    // Consumable은 같은 이름이 있으면 수량을 합산한다.
    if (item->type() == llm::ids::item::Consumable) {
        for (auto& existing : items_) {
            if (existing->name() == item->name()) {
                auto* existingConsumable = static_cast<Consumable*>(existing.get());
                auto* newConsumable      = static_cast<Consumable*>(item.get());
                // quantity_ 직접 접근 대신 consume 역방향은 없으므로 새 객체로 교체한다.
                auto merged = std::make_unique<Consumable>(
                    existing->name(), existing->description(), existing->value(),
                    existingConsumable->hpRestore(),
                    existingConsumable->quantity() + newConsumable->quantity());
                existing = std::move(merged);
                return true;
            }
        }
    }

    if (isFull()) {
        return false;
    }

    items_.push_back(std::move(item));
    return true;
}

bool Inventory::addLLMItem(const llm::Item& item)
{
    return addItem(Item::fromLLMItem(item));
}

bool Inventory::removeItem(const std::string& name)
{
    auto it = std::find_if(items_.begin(), items_.end(),
        [&name](const std::unique_ptr<Item>& item) {
            return item->name() == name;
        });

    if (it == items_.end()) {
        return false;
    }

    if ((*it)->type() == llm::ids::item::Consumable) {
        auto* consumable = static_cast<Consumable*>(it->get());
        const bool hasRemaining = consumable->consume();
        if (!hasRemaining) {
            items_.erase(it);
        }
    } else {
        items_.erase(it);
    }

    return true;
}

Item* Inventory::findItem(const std::string& name)
{
    auto it = std::find_if(items_.begin(), items_.end(),
        [&name](const std::unique_ptr<Item>& item) {
            return item->name() == name;
        });
    return it != items_.end() ? it->get() : nullptr;
}

const Item* Inventory::findItem(const std::string& name) const
{
    auto it = std::find_if(items_.begin(), items_.end(),
        [&name](const std::unique_ptr<Item>& item) {
            return item->name() == name;
        });
    return it != items_.end() ? it->get() : nullptr;
}

std::vector<Equipment*> Inventory::getEquipments()
{
    std::vector<Equipment*> result;
    for (auto& item : items_) {
        if (item->type() == llm::ids::item::Weapon
            || item->type() == llm::ids::item::Armor) {
            result.push_back(static_cast<Equipment*>(item.get()));
        }
    }
    return result;
}

std::vector<Consumable*> Inventory::getConsumables()
{
    std::vector<Consumable*> result;
    for (auto& item : items_) {
        if (item->type() == llm::ids::item::Consumable) {
            result.push_back(static_cast<Consumable*>(item.get()));
        }
    }
    return result;
}

std::vector<std::string> Inventory::getItemNames() const
{
    std::vector<std::string> names;
    names.reserve(items_.size());
    for (const auto& item : items_) {
        names.push_back(item->name());
    }
    return names;
}

std::vector<std::string> Inventory::summaries(std::size_t maxItems) const
{
    std::vector<std::string> result;
    const auto limit = maxItems == 0 ? items_.size() : std::min(maxItems, items_.size());
    result.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
        result.push_back(items_[i]->summary());
    }
    return result;
}

std::vector<llm::Item> Inventory::toLLMItems() const
{
    std::vector<llm::Item> result;
    for (const auto& item : items_) {
        llm::Item converted {
            item->name(),
            item->type(),
            item->description(),
            item->value(),
        };

        if (item->type() == llm::ids::item::Consumable) {
            const auto* consumable = static_cast<const Consumable*>(item.get());
            for (int count = 0; count < std::max(1, consumable->quantity()); ++count) {
                result.push_back(converted);
            }
            continue;
        }

        result.push_back(converted);
    }
    return result;
}

} // namespace textrpg::game
