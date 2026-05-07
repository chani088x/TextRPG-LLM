#pragma once

#include "game/Item.hpp"

namespace textrpg::game {

class Consumable : public Item {
public:
    Consumable(std::string name, std::string description, int value,
               int hpRestore, int quantity = 1);

    std::string   type()    const override;
    std::string   summary() const override;

    int  hpRestore() const { return hpRestore_; }
    int  quantity()  const { return quantity_; }

    // 한 개 소모. 수량이 남아있으면 true, 0이 되면 false를 반환한다.
    bool consume();

private:
    int hpRestore_;
    int quantity_;
};

} // namespace textrpg::game
