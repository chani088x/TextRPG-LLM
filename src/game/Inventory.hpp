#pragma once

#include "game/Consumable.hpp"
#include "game/Equipment.hpp"
#include "game/Item.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace textrpg::game {

class Inventory {
public:
    static constexpr int MAX_SLOTS = 20;

    // 아이템 추가. Consumable은 같은 이름끼리 수량을 합산한다.
    // 슬롯이 가득 차면 false를 반환한다.
    bool addItem(std::unique_ptr<Item> item);

    // 이름으로 아이템 한 개 제거 (Consumable이면 수량만 감소).
    // 없으면 false를 반환한다.
    bool removeItem(const std::string& name);

    // 소유권 없이 조회. 없으면 nullptr.
    Item*       findItem(const std::string& name);
    const Item* findItem(const std::string& name) const;

    // 타입별 조회 (다형성 활용)
    std::vector<Equipment*>  getEquipments();
    std::vector<Consumable*> getConsumables();

    // LLM PlayerSnapshot 전달용 이름 목록
    std::vector<std::string> getItemNames() const;

    const std::vector<std::unique_ptr<Item>>& items() const { return items_; }
    int  size()   const { return static_cast<int>(items_.size()); }
    bool isFull() const { return size() >= MAX_SLOTS; }

private:
    std::vector<std::unique_ptr<Item>> items_;
};

} // namespace textrpg::game
