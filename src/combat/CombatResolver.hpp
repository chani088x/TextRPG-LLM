#pragma once

#include "combat/CombatTypes.hpp"
#include "llm/LLMTypes.hpp"

#include <optional>
#include <string>
#include <vector>

namespace textrpg::combat {

    struct CombatResolution {
        std::string logText;
        std::string actionContext;
    };

    class CombatResolver {
    public:
        bool isActive() const;
        void updateFromEvent(const llm::GameEvent& event);

        // UI 동적 렌더링을 위해 보유 중인 스킬 목록을 노출하는 API 인터페이스 사양 추가
        std::vector<std::string> getPlayerSkillNames() const;

        // 선택한 스킬 인덱스를 기반으로 엔진의 데이터 상태 변화를 처리하는 시그니처 변경
        CombatResolution resolvePlayerTurn(llm::GameState& state, int skillIndex);

        CombatResolution resolveCustomAction(
            llm::GameState& state,
            const std::string& actionText,
            const std::string& diceOutcome,
            int diceValue);
        CombatResolution resolveItemUse(llm::GameState& state);

    private:
        std::optional<llm::Monster> activeMonsterLlmData_;

        // 핵심 소유(Ownership) 필드: 연속적인 전투 인스턴스 정보 유지 보존
        std::optional<Combatant> activePlayer_;
        std::optional<Combatant> activeMonster_;
    };

} // namespace textrpg::combat