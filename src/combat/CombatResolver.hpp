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
        void updateFromEvent(const llm::GameEvent& event, const llm::BossInfo& boss);

        // 직전(혹은 진행 중) 전투가 최종 보스전이고, 그 전투에서 플레이어가 승리했는지.
        // 보스를 쓰러뜨리면 엔진이 이 값으로 게임 종료를 직접 결정한다(LLM 판단에 의존하지 않음).
        bool bossDefeated() const;

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

        bool bossFight_ = false;     // 현재 전투가 최종 보스전인가 (전투 시작 시 판정)
        bool bossDefeated_ = false;  // 보스전을 승리로 끝냈는가 (게임 종료 신호)
    };

} // namespace textrpg::combat