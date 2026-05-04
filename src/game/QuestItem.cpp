#include "game/QuestItem.hpp"

namespace textrpg::game {

QuestItem::QuestItem(std::string name, std::string description, int value)
    : Item(std::move(name), std::move(description), value)
{
}

llm::ItemType QuestItem::type() const
{
    return llm::ItemType::QuestItem;
}

std::string QuestItem::summary() const
{
    return name_ + " [퀘스트]";
}

} // namespace textrpg::game
