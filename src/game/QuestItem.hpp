#pragma once

#include "game/Item.hpp"

namespace textrpg::game {

class QuestItem : public Item {
public:
    QuestItem(std::string name, std::string description, int value);

    llm::ItemType type()    const override;
    std::string   summary() const override;
};

} // namespace textrpg::game
