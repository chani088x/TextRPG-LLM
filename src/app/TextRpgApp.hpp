#pragma once

#include "app/CliView.hpp"
#include "combat/CombatResolver.hpp"
#include "game/DangerSystem.hpp"
#include "llm/LLM.hpp"
#include "NPC/NPCManager.hpp"

#include <optional>
#include <string>
#include <vector>

namespace textrpg::app {

    struct AppConfig {
        llm::LLMOptions llmOptions;
        std::vector<std::string> scriptedInputs;
        std::string gameDataPath = "data/game_state.json";
        std::string recordsJsonPath = "data/game_records.json";
    };

    AppConfig parseAppConfig();

    class TextRpgApp {
    public:
        explicit TextRpgApp(AppConfig config);

        int run();

    private:
        struct PlayerAction {
            bool isCustom = false;
            bool isElderDialogue = false;
            bool isBaseUnlock = false;
            enum class BaseService {
                Shop,
                BuyPotion,
                BuyWeapon,
                BuyArmor,
                Inn,
                UpgradeWeapon,
                UpgradeArmor,
            };
            std::string customText;
            std::string actionContext;
            std::optional<llm::CombatChoice> combatChoice;
            std::optional<BaseService> baseService;

            // 새로 추가된 부분: 스킬 메뉴 및 인덱스 기억
            int skillIndex = 0;
            bool showSkillMenu = false;
        };

        void loadGameData();
        void loadRecordsJson();
        void saveRecordsJson();
        void captureRecordSnapshot();
        bool restoreSavedState();

        void printStatus() const;
        void printEvent(const llm::GameEvent& event) const;
        void printActionResult(const llm::ActionResult& result) const;
        void printElderDialogue(const llm::ElderDialogueResult& result) const;
        void printChoiceMenu() const;

        ActionInputInfo makeActionInputInfo() const;
        std::string executeTurn(const std::string& input, bool interactive, bool& gameEnded);
        std::string readInlineInput(const std::string& prompt);
        std::string readPlayerInput();

        PlayerAction parsePlayerAction(std::string input) const;
        std::string resolveDefaultAction(const PlayerAction& action);
        std::string resolveBaseService(PlayerAction::BaseService service);

        bool maybePromptBaseUnlock(const std::string& triggerText);
        bool canTalkToElder() const;
        bool canSetBaseHere() const;
        bool canUseBaseServices() const;
        bool wasBaseDeclinedAtLocation() const;
        void markBaseDeclined();
        void unlockBase();

        bool scriptedInputFinished() const;

        AppConfig config_;
        llm::LLM llm_;
        llm::GameState state_;
        CliView view_;
        combat::CombatResolver combat_;
        textrpg::game::DangerSystem danger_;
        textrpg::npc::NPCManager npcManager_;
        std::size_t scriptedIndex_ = 0;
        std::vector<std::string> uiChatHistory_;

        // 새로 추가된 부분: 스킬 입력을 기다리는 상태인지 확인
        bool waitingForSkillSelection_ = false;
    };

} // namespace textrpg::app