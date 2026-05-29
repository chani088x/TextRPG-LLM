#include "combat/CombatResolver.hpp"

#include "combat/CombatSystem.hpp"
#include "game/Inventory.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <vector>

namespace textrpg::combat {
    namespace {

        void pushLimited(std::vector<std::string>& values, const std::string& value, std::size_t maxCount)
        {
            if (value.empty()) {
                return;
            }

            values.push_back(value);
            while (values.size() > maxCount) {
                values.erase(values.begin());
            }
        }

        std::string formatCombatResult(const CombatResult& result)
        {
            std::ostringstream out;
            out << "공방이 이어졌다.\n";
            for (const auto& turn : result.turns) {
                out << turn.description << '\n';
            }

            if (result.finished) {
                out << (result.winner == CombatWinner::Player
                    ? "적이 쓰러졌다."
                    : "플레이어가 쓰러졌다.")
                    << '\n';
            }
            else {
                out << "전투가 계속된다. "
                    << result.monster.getName() << " HP " << result.monster.getHp()
                    << ", 플레이어 HP " << result.player.getHp() << '\n';
            }
            return out.str();
        }

        std::string summarizeCombatState(const CombatResult& result)
        {
            if (result.finished) {
                if (result.winner == CombatWinner::Player) {
                    return "플레이어가 " + result.monster.getName() + "를 쓰러뜨렸다.";
                }
                return result.monster.getName() + "에게 쓰러졌다.";
            }

            std::ostringstream out;
            out << result.monster.getName() << "와 전투 중이다. 적 HP "
                << result.monster.getHp() << ", 플레이어 HP " << result.player.getHp() << '.';
            return out.str();
        }

        int customDamageForOutcome(const llm::GameState& state, const std::string& outcome)
        {
            if (outcome == llm::ids::dice::Jackpot) {
                return std::max(8, state.player.attack * 2);
            }
            if (outcome == llm::ids::dice::Success) {
                return std::max(4, state.player.attack);
            }
            return 1;
        }

        std::optional<llm::Item> findUsableConsumable(game::Inventory& inventory)
        {
            auto consumables = inventory.getConsumables();
            if (consumables.empty()) {
                return std::nullopt;
            }

            const auto* item = consumables.back();
            return llm::Item{
                item->name(),
                item->type(),
                item->description(),
                item->value(),
            };
        }

        int consumableHealAmount(game::Inventory& inventory, const std::string& name)
        {
            for (const auto* item : inventory.getConsumables()) {
                if (item->name() == name) {
                    return std::max(1, item->hpRestore());
                }
            }
            return 1;
        }

    } // namespace

    bool CombatResolver::isActive() const
    {
        return activeMonsterLlmData_.has_value();
    }

    void CombatResolver::updateFromEvent(const llm::GameEvent& event)
    {
        if (event.eventType == llm::ids::event::Combat && event.monster.has_value()) {
            activeMonsterLlmData_ = event.monster;

            // 전투 세션 최초 생성 시 플레이어 소유 객체 구성
            if (!activePlayer_.has_value()) {
                activePlayer_ = makeDefaultPlayer();
            }

            // 전투 돌입 순간 플레이어 체력 값을 엔진 코어 스탯과 매칭
            activePlayer_->setHp(event.statChanges.hp > 0 ? event.statChanges.hp : activePlayer_->getHp());

            // 매 만남마다 새로운 몬스터 인스턴스 생성 및 상태 보존 시작
            activeMonster_ = makeDefaultMonster(event.monster->name);
            activeMonster_->setHp(std::max(1, event.monster->hp));
            return;
        }

        activeMonsterLlmData_.reset();
        activeMonster_.reset();
    }

    std::vector<std::string> CombatResolver::getPlayerSkillNames() const
    {
        std::vector<std::string> names;
        if (activePlayer_.has_value()) {
            for (const auto& skill : activePlayer_->getSkills()) {
                names.push_back(skill->getName());
            }
        }
        return names;
    }

    CombatResolution CombatResolver::resolvePlayerTurn(llm::GameState& state, int skillIndex)
    {
        if (!activeMonster_.has_value() || !activePlayer_.has_value()) {
            return { "", "플레이어는 공격 자세를 취했지만 눈앞에 확실한 적은 없었다." };
        }

        // 최신 코어 체력을 로컬 인스턴스에 강제 매칭
        activePlayer_->setHp(state.player.hp);

        CombatSystem combatSystem;
        const auto result = combatSystem.runRound(activePlayer_.value(), activeMonster_.value(), skillIndex);

        // 라운드 정산 연산값을 코어 GameState 엔진에 완벽 보존 동기화
        state.player.hp = activePlayer_->getHp();

        if (result.finished) {
            activeMonsterLlmData_.reset();
            activeMonster_.reset();
        }
        else {
            if (activeMonsterLlmData_.has_value()) {
                activeMonsterLlmData_->hp = activeMonster_->getHp();
            }
        }

        const auto summary = summarizeCombatState(result);
        pushLimited(state.memory.recentEvents, "전투: " + summary, 5);

        return { formatCombatResult(result), summary };
    }

    CombatResolution CombatResolver::resolveCustomAction(
        llm::GameState& state,
        const std::string& actionText,
        const std::string& diceOutcome,
        int diceValue)
    {
        if (!activeMonster_.has_value() || !activePlayer_.has_value()) {
            return { "", "플레이어는 " + actionText + "을(를) 시도했지만 눈앞에 확실한 적은 없었다." };
        }

        activePlayer_->setHp(state.player.hp);
        int damage = customDamageForOutcome(state, diceOutcome);
        int actualDamage = activeMonster_->receiveDamage(damage);

        std::ostringstream log;
        log << "플레이어의 고유 행동 [" << actionText << "]이(d12 " << diceValue << ") 발동!\n";
        log << activePlayer_->getName() << "이(가) 고유 행동으로 " << actualDamage << "의 피해를 입혔습니다.\n";

        if (!activeMonster_->isDead()) {
            int mChoice = 0;
            if (!activeMonster_->getSkills().empty()) {
                mChoice = std::rand() % activeMonster_->getSkills().size();
            }
            CombatResult monsterTurnResult;
            activeMonster_->getSkills()[mChoice]->execute(CombatActor::Monster, *activeMonster_, *activePlayer_, monsterTurnResult);
            for (const auto& t : monsterTurnResult.turns) {
                log << t.description << '\n';
            }
        }

        state.player.hp = activePlayer_->getHp();

        CombatResult finalResult;
        finalResult.player = *activePlayer_;
        finalResult.monster = *activeMonster_;
        if (activePlayer_->isDead() || activeMonster_->isDead()) {
            finalResult.finished = true;
            finalResult.winner = (!activePlayer_->isDead()) ? CombatWinner::Player : CombatWinner::Monster;
            activeMonsterLlmData_.reset();
            activeMonster_.reset();
        }
        else {
            finalResult.finished = false;
            if (activeMonsterLlmData_.has_value()) {
                activeMonsterLlmData_->hp = activeMonster_->getHp();
            }
        }

        const auto summary = summarizeCombatState(finalResult);
        pushLimited(state.memory.recentEvents, "전투: " + summary, 5);

        return { log.str() + "\n" + formatCombatResult(finalResult), summary };
    }

    CombatResolution CombatResolver::resolveItemUse(llm::GameState& state)
    {
        auto inventory = game::Inventory::fromLLMItems(state.player.inventory);
        const auto item = findUsableConsumable(inventory);
        if (!item.has_value()) {
            return { "", "플레이어는 [아이템]을 확인했지만 사용할 소모품이 없다." };
        }

        if (state.player.hp >= state.player.maxHp) {
            return { "", "플레이어는 " + item->name + "을 확인했지만 HP가 이미 가득 차 있어 아껴 두었다." };
        }

        const int beforeHp = state.player.hp;
        const int healAmount = consumableHealAmount(inventory, item->name);
        state.player.hp = llm::clampInt(state.player.hp + healAmount, 1, state.player.maxHp);
        inventory.removeItem(item->name);
        state.player.inventory = inventory.toLLMItems();

        std::ostringstream out;
        out << "플레이어는 [아이템] " << item->name << "을 사용했다. HP "
            << beforeHp << " -> " << state.player.hp << ".\n";
        pushLimited(state.memory.recentEvents, "아이템 사용: " + item->name, 5);

        if (activeMonster_.has_value() && activePlayer_.has_value()) {
            activePlayer_->setHp(state.player.hp);

            int mChoice = 0;
            if (!activeMonster_->getSkills().empty()) {
                mChoice = std::rand() % activeMonster_->getSkills().size();
            }
            CombatResult monsterTurnResult;
            activeMonster_->getSkills()[mChoice]->execute(CombatActor::Monster, *activeMonster_, *activePlayer_, monsterTurnResult);

            state.player.hp = activePlayer_->getHp();

            CombatResult finalResult;
            finalResult.player = *activePlayer_;
            finalResult.monster = *activeMonster_;

            if (activePlayer_->isDead() || activeMonster_->isDead()) {
                finalResult.finished = true;
                finalResult.winner = (!activePlayer_->isDead()) ? CombatWinner::Player : CombatWinner::Monster;
                activeMonsterLlmData_.reset();
                activeMonster_.reset();
            }
            else {
                finalResult.finished = false;
                if (activeMonsterLlmData_.has_value()) {
                    activeMonsterLlmData_->hp = activeMonster_->getHp();
                }
            }

            out << formatCombatResult(finalResult);
            pushLimited(state.memory.recentEvents, "전투: " + summarizeCombatState(finalResult), 5);
        }

        return { "", out.str() };
    }

} // namespace textrpg::combat