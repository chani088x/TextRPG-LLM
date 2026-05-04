#pragma once

#include "llm/LLMTypes.hpp"

#include <memory>
#include <string>

namespace textrpg::game {

class Item {
public:
    Item(std::string name, std::string description, int value);
    virtual ~Item() = default;

    Item(const Item&)            = default;
    Item& operator=(const Item&) = default;
    Item(Item&&)                 = default;
    Item& operator=(Item&&)      = default;

    const std::string& name()        const { return name_; }
    const std::string& description() const { return description_; }
    int                value()        const { return value_; }

    virtual llm::ItemType type()    const = 0;
    virtual std::string   summary() const = 0;

    // llm::Item -> game::Item 변환. type에 따라 적절한 서브클래스를 생성한다.
    static std::unique_ptr<Item> fromLLMItem(const llm::Item& llmItem);

protected:
    std::string name_;
    std::string description_;
    int         value_;
};

} // namespace textrpg::game
