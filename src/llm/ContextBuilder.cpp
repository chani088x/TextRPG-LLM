#include "llm/ContextBuilder.hpp"

#include <sstream>
#include <utility>

namespace textrpg::llm {
namespace {

void appendLimitedList(
    std::ostringstream& out,
    const std::vector<std::string>& values,
    std::size_t maxItems,
    const std::string& emptyText)
{
    // 최근 항목만 넘겨 context가 장기 로그로 비대해지는 것을 막는다.
    if (values.empty()) {
        out << "- " << emptyText << '\n';
        return;
    }

    const auto start = values.size() > maxItems ? values.size() - maxItems : 0;
    for (std::size_t i = start; i < values.size(); ++i) {
        out << "- " << values[i] << '\n';
    }
}

} // namespace

ContextBuilder::ContextBuilder(PromptSettings settings)
    : settings_(std::move(settings))
{
}

std::string ContextBuilder::build(const GameState& state, const std::string& playerInput) const
{
    // LLM에는 엔진 내부 구조가 아니라 판단에 필요한 요약 정보만 제공한다.
    std::ostringstream out;
    out << "턴: " << state.turnNumber << '\n';
    out << "현재 위치: " << state.world.location << '\n';
    out << "현재 장면: "
        << (state.currentScene.empty() ? "아직 구체적인 장면이 정해지지 않았다." : state.currentScene)
        << '\n';
    out << "진행 중인 목표: " << state.world.currentObjective << '\n';
    out << "현재 판단 기준: " << state.world.decisionHint << '\n';
    out << '\n';

    out << "[플레이어 요약]\n";
    out << "- HP: " << state.player.hp << "/" << state.player.maxHp << '\n';
    out << "- 레벨: " << state.player.level << '\n';
    out << "- 공격/방어: " << state.player.attack << "/" << state.player.defense << '\n';
    out << "- 골드/경험치: " << state.player.gold << "/" << state.player.exp << '\n';
    out << "- 핵심 인벤토리:\n";
    appendLimitedList(out, state.player.inventory, settings_.maxInventoryItems, "소지품 정보 없음");
    out << '\n';

    out << "[최근 사건]\n";
    appendLimitedList(out, state.memory.recentEvents, settings_.maxRecentEvents, "아직 기록된 사건 없음");
    out << '\n';

    out << "[중요한 선택 이력]\n";
    appendLimitedList(out, state.memory.importantChoices, settings_.maxRecentEvents, "아직 기록된 선택 없음");
    out << '\n';

    out << "[세계관 고정 규칙]\n";
    if (state.world.fixedRules.empty()) {
        out << "- 배경은 중세 판타지 세계이며 현대 기술과 SF 요소는 등장하지 않는다.\n";
        out << "- 플레이어를 즉사시키거나 무한 보상을 지급하지 않는다.\n";
    } else {
        appendLimitedList(out, state.world.fixedRules, state.world.fixedRules.size(), "고정 규칙 없음");
    }
    out << '\n';

    out << "최근 플레이어 입력: " << playerInput << '\n';

    return out.str();
}

} // namespace textrpg::llm
