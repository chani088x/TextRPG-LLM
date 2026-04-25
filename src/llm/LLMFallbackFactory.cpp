#include "llm/LLMFallbackFactory.hpp"

namespace textrpg::llm {

GameEvent LLMFallbackFactory::createSafeEvent(const std::string& reason) const
{
    GameEvent event;
    event.sceneText = "잠시 주변이 조용해졌다. 당신은 숨을 고르며 다음 행동을 고민한다.";
    event.eventType = EventType::Story;
    event.nextObjective = "주변 상황을 다시 확인하고 안전한 단서를 찾는다";
    event.decisionHint = "정보를 더 모으면 안전하지만 시간이 지나며 위협이 가까워질 수 있다";
    event.choices = {
        "주변을 조사한다",
        "앞으로 나아간다",
        "잠시 휴식한다",
    };
    event.monster.reset();
    event.item.reset();
    event.statChanges = {};
    event.memoryNote = "LLM 응답 오류로 인해 안전한 기본 이벤트가 생성되었다.";
    event.usedFallback = true;
    event.validationNotes.push_back(reason);
    return event;
}

} // namespace textrpg::llm
