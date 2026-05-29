#include "app/TextRpgApp.hpp"

#include "app/CliView.hpp"
#include "app/SoundManager.hpp"
#include "combat/CombatResolver.hpp"
#include "game/DangerSystem.hpp"
#include "game/Inventory.hpp"
#include "game/Item.hpp"
#include "llm/RecordStore.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <utility>

namespace textrpg::app {

    using namespace textrpg::llm;

    namespace {

        std::string toLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
                });
            return value;
        }

        bool tryParseProvider(const std::string& value, LLMProvider& provider)
        {
            const auto normalized = toLower(value);
            if (normalized == "ollama") {
                provider = LLMProvider::Ollama;
                return true;
            }
            return false;
        }

        std::string defaultModel()
        {
            return "aravhawk/gemma4:26b";
        }

        std::string defaultEndpoint()
        {
            return "http://localhost:11434";
        }

        std::vector<std::string> stringArrayFromJson(const nlohmann::json& value)
        {
            std::vector<std::string> values;
            if (!value.is_array()) {
                return values;
            }
            for (const auto& item : value) {
                if (item.is_string()) {
                    values.push_back(item.get<std::string>());
                }
            }
            return values;
        }

        LLMOptions makeDefaultLlmOptions()
        {
            LLMOptions options;
            options.provider = LLMProvider::Ollama;
            options.endpoint = defaultEndpoint();
            options.model = defaultModel();
            return options;
        }

        void applyLlmConfig(const nlohmann::json& value, LLMOptions& options)
        {
            if (!value.is_object()) {
                return;
            }

            LLMProvider provider;
            if (tryParseProvider(value.value("provider", std::string("ollama")), provider)) {
                options.provider = provider;
            }
            options.endpoint = value.value("endpoint", options.endpoint.empty() ? defaultEndpoint() : options.endpoint);
            options.model = value.value("model", options.model.empty() ? defaultModel() : options.model);
            options.temperature = value.value("temperature", options.temperature);
            options.think = value.value("think", options.think);
            options.connectionTimeoutSeconds = value.value("connection_timeout_seconds", options.connectionTimeoutSeconds);
            options.readTimeoutSeconds = value.value("read_timeout_seconds", options.readTimeoutSeconds);
        }

        void loadConfigJson(const std::string& path, AppConfig& config)
        {
            if (!std::filesystem::exists(path)) {
                return;
            }

            std::ifstream input(path);
            if (!input) {
                return;
            }

            try {
                nlohmann::json root;
                input >> root;
                if (!root.is_object()) {
                    return;
                }

                applyLlmConfig(root.value("llm", nlohmann::json::object()), config.llmOptions);
                config.gameDataPath = root.value("game_data_path", config.gameDataPath);
                config.recordsJsonPath = root.value("records_json_path", config.recordsJsonPath);
                config.scriptedInputs = stringArrayFromJson(root.value("scripted_inputs", nlohmann::json::array()));
            }
            catch (...) {
                std::cout << "설정 파일을 읽지 못했습니다. 기본 설정으로 실행합니다: " << path << '\n';
            }
        }

        GameState makeInitialState()
        {
            GameState state;
            state.turnNumber = 1;
            return state;
        }

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

        bool containsText(const std::string& text, const std::string& needle)
        {
            return !needle.empty() && text.find(needle) != std::string::npos;
        }

        bool isAffirmative(std::string input)
        {
            input = toLower(input);
            return input == "y" || input == "yes" || input == "예" || input == "네" || input == "ㅇ" || input == "응";
        }

        bool isNegative(std::string input)
        {
            input = toLower(input);
            return input == "n" || input == "no" || input == "아니오" || input == "아니요" || input == "ㄴ" || input == "싫어";
        }

        bool isExitCommand(const std::string& input)
        {
            return input == "q" || input == "quit" || input == "exit" || input == "종료";
        }

        bool isUnsignedInteger(const std::string& input)
        {
            return !input.empty()
                && std::all_of(input.begin(), input.end(), [](unsigned char ch) {
                return std::isdigit(ch) != 0;
                    });
        }

        std::pair<int, std::string> rollD12()
        {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<int> distribution(1, 12);
            const int value = distribution(rng);
            if (value >= 11) {
                return { value, ids::dice::Jackpot };
            }
            if (value >= 6) {
                return { value, ids::dice::Success };
            }
            return { value, ids::dice::Failure };
        }

        std::string dangerContext(const textrpg::game::DangerAdvance& danger)
        {
            std::ostringstream out;
            out << "위험도 판정: " << danger.previous << " + " << danger.increase
                << " = " << danger.current << "/" << danger.threshold << ". ";
            if (danger.combatTriggered) {
                out << "위험도가 10 이상이 되어 엔진이 전투 이벤트를 강제한다.";
            }
            else {
                out << "위험도가 아직 10 미만이므로 combat 이벤트는 금지된다.";
            }
            return out.str();
        }

        void printLineByLine(const std::string& text)
        {
            std::istringstream lines(text);
            std::string line;
            while (std::getline(lines, line)) {
                if (!line.empty()) {
                    std::cout << line << '\n';
                }
            }
        }

        bool isPrologueRecentEvent(const std::string& value)
        {
            return value.rfind("프롤로그:", 0) == 0;
        }

        bool hasMainObjective(const GameRecords& records)
        {
            return records.elder.talked || records.boss.known;
        }

        void erasePrologueRecentEvents(std::vector<std::string>& values)
        {
            values.erase(
                std::remove_if(values.begin(), values.end(), isPrologueRecentEvent),
                values.end());
        }

        void removePreElderObjectives(GameState& state)
        {
            if (!hasMainObjective(state.records)) {
                state.records.questLog.clear();
            }
        }

        std::string trimDisplayText(std::string value)
        {
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
                value.erase(value.begin());
            }
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
                value.pop_back();
            }
            return value;
        }

        bool startsWithAny(const std::string& text, const std::vector<std::string>& prefixes)
        {
            return std::any_of(prefixes.begin(), prefixes.end(), [&text](const std::string& prefix) {
                return text.rfind(prefix, 0) == 0;
                });
        }

        std::string cleanDisplayText(const std::string& text)
        {
            std::istringstream lines(text);
            std::ostringstream out;
            std::string line;
            bool wroteLine = false;

            const std::vector<std::string> hiddenPrefixes = {
                "현재 지역:",
                "상황:",
                "변화:",
                "event_type",
                "memory_note",
                "stat_changes",
                "next_event_hint",
            };

            while (std::getline(lines, line)) {
                line = trimDisplayText(line);
                if (line.empty() || startsWithAny(line, hiddenPrefixes)) {
                    continue;
                }
                if (wroteLine) {
                    out << '\n';
                }
                out << line;
                wroteLine = true;
            }
            return out.str();
        }

        std::string formatSignedChange(const std::string& label, int value)
        {
            std::ostringstream out;
            out << label << ' ';
            if (value > 0) {
                out << '+';
            }
            out << value;
            return out.str();
        }

        void appendUiBlock(std::ostringstream& out, const std::string& title, const std::string& body)
        {
            const auto cleaned = cleanDisplayText(body);
            if (cleaned.empty()) {
                return;
            }
            if (out.tellp() > 0) {
                out << "\n\n";
            }
            out << '[' << title << "]\n" << cleaned;
        }

        void rememberObtainedItem(GameState& state, const Item& item)
        {
            if (item.name.empty()) {
                return;
            }

            const auto sameName = [&item](const Item& existing) {
                return existing.name == item.name;
                };
            if (std::find_if(state.records.obtainedItems.begin(), state.records.obtainedItems.end(), sameName)
                == state.records.obtainedItems.end()) {
                state.records.obtainedItems.push_back(item);
            }
        }

        std::string summarizeGameItem(const Item& item)
        {
            const auto gameItem = textrpg::game::Item::fromLLMItem(item);
            return gameItem ? gameItem->summary() : item.name;
        }

        Item shopPotion()
        {
            return Item{
                "회복 물약",
                ids::item::Consumable,
                "거점 상인이 파는 기본 회복약.",
                10,
            };
        }

        Item shopWeapon()
        {
            return Item{
                "수련용 검",
                ids::item::Weapon,
                "날은 평범하지만 손에 익히기 쉬운 검.",
                20,
            };
        }

        Item shopArmor()
        {
            return Item{
                "가죽 조끼",
                ids::item::Armor,
                "급소를 가려 주는 가벼운 방어구.",
                20,
            };
        }

        std::string formatStatChanges(const StatChanges& changes)
        {
            std::vector<std::string> parts;
            if (changes.hp != 0) {
                parts.push_back(formatSignedChange("HP", changes.hp));
            }
            if (changes.gold != 0) {
                parts.push_back(formatSignedChange("Gold", changes.gold));
            }
            if (changes.exp != 0) {
                parts.push_back(formatSignedChange("Exp", changes.exp));
            }
            if (parts.empty()) {
                return {};
            }

            std::ostringstream out;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << parts[i];
            }
            return out.str();
        }

        std::string formatDiceRollForUi(int value, const std::string& outcome)
        {
            std::ostringstream out;
            out << diceOutcomeToKorean(outcome) << " (d12 " << value << ')';
            return out.str();
        }

        StatChanges addStatChanges(const StatChanges& lhs, const StatChanges& rhs)
        {
            return StatChanges{
                lhs.hp + rhs.hp,
                lhs.gold + rhs.gold,
                lhs.exp + rhs.exp,
            };
        }

        bool eventStatsAreApplied(const GameEvent& event)
        {
            return event.eventType != ids::event::Combat;
        }

        std::string formatItemGain(const std::optional<Item>& item)
        {
            if (!item.has_value()) {
                return {};
            }

            std::ostringstream out;
            out << "\n얻은 물건: " << item->name << '\n';
            if (!item->description.empty()) {
                out << cleanDisplayText(item->description) << '\n';
            }
            return out.str();
        }

        std::string formatActionResultForUi(const ActionResult& result)
        {
            std::ostringstream out;
            const auto body = cleanDisplayText(result.resultText);
            if (!body.empty()) {
                out << body;
            }
            return out.str();
        }

        std::string formatEventForUi(const GameEvent& event, bool mainObjectiveKnown)
        {
            (void)mainObjectiveKnown;
            std::ostringstream out;
            const auto story = cleanDisplayText(event.sceneText);
            const auto situation = cleanDisplayText(event.decisionHint);
            if (!story.empty()) {
                out << story;
            }
            if (!situation.empty()) {
                if (out.tellp() > 0) {
                    out << "\n\n";
                }
                out << situation;
            }
            return out.str();
        }

        std::string formatElderDialogueForUi(const ElderDialogueResult& result)
        {
            std::ostringstream out;
            std::istringstream dialogue(cleanDisplayText(result.dialogue));
            std::string line;
            while (std::getline(dialogue, line)) {
                const auto cleaned = cleanDisplayText(line);
                if (cleaned.empty()) {
                    continue;
                }
                if (out.tellp() > 0) {
                    out << "\n\n";
                }
                out << cleaned;
            }

            std::ostringstream info;
            info << "이름: " << result.boss.name << '\n';
            info << "위치: " << result.boss.location << '\n';
            info << "약점: " << result.boss.weakness;
            if (!result.boss.description.empty()) {
                info << '\n' << cleanDisplayText(result.boss.description);
            }
            if (!result.questUpdate.empty()) {
                info << "\n이제 향할 곳: " << cleanDisplayText(result.questUpdate);
            }
            appendUiBlock(out, "정보", info.str());
            return out.str();
        }

    } // namespace

    AppConfig parseAppConfig()
    {
        AppConfig config;
        config.llmOptions = makeDefaultLlmOptions();
        loadConfigJson("data/config.json", config);
        return config;
    }

    TextRpgApp::TextRpgApp(AppConfig config)
        : config_(std::move(config))
        , llm_(config_.llmOptions)
        , state_(makeInitialState())
    {
        npcManager_.addNPC("장로", { "장로", "elder" }, "조심스럽고 오래 기억하는 사람", "낮고 단정한 말투", "거점");
        npcManager_.addNPC("상인", { "상인", "상점", "거래", "merchant" }, "계산이 빠르지만 거점 사람에게는 협조적", "짧고 실속 있는 말투", "거점 상점");
        npcManager_.addNPC("여관주인", { "여관주인", "여관", "휴식", "innkeeper" }, "피곤한 여행자를 잘 살피는 사람", "부드럽고 담백한 말투", "거점 여관");
        npcManager_.addNPC("대장장이", { "대장장이", "강화", "장비", "blacksmith" }, "말수는 적지만 손재주가 좋은 장인", "투박하고 직설적인 말투", "거점 대장간");
    }

    int TextRpgApp::run()
    {
        loadGameData();
        bool restoredSavedState = false;

        if (config_.scriptedInputs.empty()) {
            const bool hasSave = std::filesystem::exists(config_.recordsJsonPath);
            const auto menuChoice = view_.showStartMenu(hasSave);

            if (menuChoice == StartMenuChoice::Exit) {
                std::cout << "게임을 종료합니다.\n";
                return 0;
            }

            if (menuChoice == StartMenuChoice::ContinueGame) {
                loadRecordsJson();
                restoredSavedState = restoreSavedState();
            }
            else {
                state_.records = {};
                std::error_code error;
                std::filesystem::remove(config_.recordsJsonPath, error);
            }

        }
        else {
            loadRecordsJson();
            restoredSavedState = restoreSavedState();
        }

        // 게임 진입 시 BGM 루프 재생
        SoundManager::instance().playBGM("data/sound/bgm.wav");

        if (!restoredSavedState) {
            if (config_.scriptedInputs.empty()) {
                view_.runWithLoadingScreen("첫 장면과 현재 세계를 만들고 있습니다.", [&] {
                    llm_.generatePrologue(state_);
                    });
            }
            else {
                llm_.generatePrologue(state_);
            }
            saveRecordsJson();

            if (config_.scriptedInputs.empty()) {
                view_.runWithLoadingScreen("프롤로그를 정리하고 시작 지점을 잡고 있습니다.", [&] {
                    llm_.generateInitialWorld(state_);
                    });
            }
            else {
                llm_.generateInitialWorld(state_);
            }
        }
        saveRecordsJson();

        while (state_.player.hp > 0) {
            saveRecordsJson();
            const bool interactive = config_.scriptedInputs.empty();

            if (interactive) {
                bool gameEnded = false;
                const auto turnViewResult = view_.runActionTurn(
                    makeActionInputInfo(),
                    [&](const std::string& input) {
                        TurnViewResult result;
                        result.status = makeActionInputInfo();
                        if (isExitCommand(input)) {
                            saveRecordsJson();
                            result.exitRequested = true;
                            return result;
                        }

                        // --- 수정된 부분 ---
                        bool wasWaiting = waitingForSkillSelection_; // 턴 실행 전의 상태 저장

                        result.body = executeTurn(input, true, gameEnded);
                        result.status = makeActionInputInfo();

                        // 방금 스킬 메뉴로 전환된 것이라면 결과창 연출 스킵
                        if (!wasWaiting && waitingForSkillSelection_) {
                            result.skipReveal = true;
                        }
                        // -------------------
                        return result;
                    });

                if (turnViewResult.exitRequested) {
                    saveRecordsJson();
                    std::cout << "게임을 종료합니다.\n";
                    return 0;
                }
                if (!turnViewResult.body.empty()) {
                    uiChatHistory_.push_back(turnViewResult.body);
                    while (uiChatHistory_.size() > 20) {
                        uiChatHistory_.erase(uiChatHistory_.begin());
                    }
                }
                if (gameEnded) {
                    std::cout << "\n게임 종료 이벤트가 발생했습니다.\n";
                    return 0;
                }
                continue;
            }

            printStatus();

            std::string input;
            if (scriptedIndex_ < config_.scriptedInputs.size()) {
                input = config_.scriptedInputs[scriptedIndex_++];
                printChoiceMenu();
                std::cout << "\n> " << input << '\n';
            }
            else {
                input = readPlayerInput();
            }

            if (isExitCommand(input)) {
                saveRecordsJson();
                std::cout << "게임을 종료합니다.\n";
                return 0;
            }

            bool gameEnded = false;
            executeTurn(input, false, gameEnded);
            if (gameEnded) {
                std::cout << "\n게임 종료 이벤트가 발생했습니다.\n";
                return 0;
            }

            if (scriptedInputFinished()) {
                std::cout << "\n스크립트 입력 실행을 마쳤습니다.\n";
                return 0;
            }

            std::cout << "\n계속하려면 Enter를 누르세요.";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

        std::cout << "플레이어 HP가 0이 되어 게임을 종료합니다.\n";
        return 0;
    }

    void TextRpgApp::loadGameData()
    {
        std::string error;
        if (!internals::loadGameStateSeedFromJson(config_.gameDataPath, state_, error)) {
            std::cout << "게임 데이터 로드 실패: " << error << '\n';
            return;
        }
    }

    void TextRpgApp::loadRecordsJson()
    {
        GameRecords loaded;
        std::string error;
        if (!internals::loadGameRecordsFromJson(config_.recordsJsonPath, loaded, error)) {
            std::cout << "기록 JSON 로드 실패: " << error << '\n';
            return;
        }

        internals::mergeGameRecords(loaded, state_.records);
        state_.records = std::move(loaded);
        if (state_.records.base.unlocked) {
            state_.records.elder.introduced = true;
        }
    }

    void TextRpgApp::captureRecordSnapshot()
    {
        erasePrologueRecentEvents(state_.memory.recentEvents);
        removePreElderObjectives(state_);

        auto& snapshot = state_.records.snapshot;
        snapshot.saved = true;
        snapshot.turnNumber = state_.turnNumber;
        snapshot.currentScene = state_.currentScene;
        snapshot.player = state_.player;
        snapshot.world = state_.world;
        snapshot.memory = state_.memory;
    }

    bool TextRpgApp::restoreSavedState()
    {
        const auto& snapshot = state_.records.snapshot;
        if (snapshot.saved) {
            const auto fixedRules = state_.world.fixedRules;
            state_.turnNumber = snapshot.turnNumber;
            state_.currentScene = snapshot.currentScene;
            state_.player = snapshot.player;
            state_.world = snapshot.world;
            if (state_.world.fixedRules.empty()) {
                state_.world.fixedRules = fixedRules;
            }
            state_.memory = snapshot.memory;
            erasePrologueRecentEvents(state_.memory.recentEvents);
            removePreElderObjectives(state_);
            return true;
        }

        const auto& prologue = state_.records.prologue;
        if (!prologue.generated) {
            return false;
        }

        state_.currentScene = prologue.text;
        if (!prologue.openingLocation.empty()) {
            state_.world.location = prologue.openingLocation;
        }
        if (!prologue.firstObjective.empty()) {
            state_.world.currentObjective = prologue.firstObjective;
        }
        erasePrologueRecentEvents(state_.memory.recentEvents);
        removePreElderObjectives(state_);
        return true;
    }

    void TextRpgApp::saveRecordsJson()
    {
        captureRecordSnapshot();
        std::string error;
        if (!internals::saveGameRecordsToJson(config_.recordsJsonPath, state_.records, error)) {
            std::cout << "기록 JSON 저장 실패: " << error << '\n';
        }
    }

    void TextRpgApp::printStatus() const
    {
        std::cout << "\n============================================================\n";
        std::cout << "턴 " << state_.turnNumber << " | " << state_.world.location
            << " | " << (combat_.isActive() ? "전투" : "비전투") << '\n';
        std::cout << "HP " << state_.player.hp << "/" << state_.player.maxHp
            << " | LV " << state_.player.level
            << " | ATK " << state_.player.attack
            << " | DEF " << state_.player.defense
            << " | Gold " << state_.player.gold
            << " | Exp " << state_.player.exp << '\n';
        std::cout << (hasMainObjective(state_.records) ? "목표: " : "실마리: ")
            << state_.world.currentObjective << '\n';
        std::cout << "상황: " << state_.world.decisionHint << '\n';
        std::cout << "위험도: " << state_.records.danger.level
            << "/" << state_.records.danger.threshold
            << " | 최근 증가 +" << state_.records.danger.lastIncrease << '\n';
        if (state_.records.base.unlocked) {
            std::cout << "거점: " << state_.records.base.location << " | 기능:";
            for (const auto& feature : state_.records.base.features) {
                std::cout << ' ' << feature;
            }
            std::cout << '\n';
            std::cout << "장로: " << (state_.records.elder.talked ? "대화 완료" : "대화 가능") << '\n';
        }
        if (state_.records.boss.known) {
            std::cout << "보스: " << state_.records.boss.name
                << " | 위치: " << state_.records.boss.location
                << " | 약점: " << state_.records.boss.weakness << '\n';
        }
        std::cout << "============================================================\n";
    }

    void TextRpgApp::printEvent(const GameEvent& event) const
    {
        if (event.eventType == ids::event::Dialogue) {
            printLineByLine(cleanDisplayText(event.sceneText));
        }
        else {
            const auto scene = cleanDisplayText(event.sceneText);
            if (!scene.empty()) {
                std::cout << "\n" << scene << "\n";
            }
        }
        if (!event.nextObjective.empty()) {
            std::cout << (hasMainObjective(state_.records) ? "\n이제 향할 곳: " : "\n다음으로 살필 일: ")
                << cleanDisplayText(event.nextObjective) << '\n';
        }
        if (event.monster.has_value()) {
            const auto& monster = event.monster.value();
            std::cout << "\n마주친 적: " << monster.name
                << " | HP " << monster.hp
                << ", ATK " << monster.attack
                << ", DEF " << monster.defense << '\n';
            if (!monster.description.empty()) {
                std::cout << cleanDisplayText(monster.description) << '\n';
            }
        }
        if (event.item.has_value()) {
            SoundManager::instance().playItemGain();
        }
        std::cout << formatItemGain(event.item);
    }

    void TextRpgApp::printActionResult(const ActionResult& result) const
    {
        std::cout << '\n';
        printLineByLine(cleanDisplayText(result.resultText));
        if (result.item.has_value()) {
            SoundManager::instance().playItemGain();
        }
        std::cout << formatItemGain(result.item);
    }

    void TextRpgApp::printElderDialogue(const ElderDialogueResult& result) const
    {
        std::cout << '\n';
        printLineByLine(cleanDisplayText(result.dialogue));
        std::cout << "\n장로가 알려준 것\n";
        std::cout << "  이름: " << result.boss.name << '\n';
        std::cout << "  위치: " << result.boss.location << '\n';
        std::cout << "  약점: " << result.boss.weakness << '\n';
        if (!result.boss.description.empty()) {
            std::cout << "  " << cleanDisplayText(result.boss.description) << '\n';
        }
        if (!result.questUpdate.empty()) {
            std::cout << "\n이제 향할 곳: " << cleanDisplayText(result.questUpdate) << '\n';
        }
    }

    void TextRpgApp::printChoiceMenu() const
    {
        if (waitingForSkillSelection_) {
            std::cout << "\n번호를 고르거나 직접 문장으로 적으세요. (종료: q)\n";
            return;
        }

        std::vector<std::string> choices;
        if (combat_.isActive()) {
            choices = {
                "1. 공격",
                "2. 아이템",
            };
        }
        else {
            int index = 1;
            if (canTalkToElder()) {
                choices.push_back(std::to_string(index++) + ". 장로와 대화");
            }
            if (canSetBaseHere()) {
                choices.push_back(std::to_string(index++) + ". 거점으로 삼기");
            }
            if (canUseBaseServices()) {
                choices.push_back(std::to_string(index++) + ". 상점 보기");
                choices.push_back(std::to_string(index++) + ". 여관에서 휴식");
                choices.push_back(std::to_string(index++) + ". 장비 강화");
            }
        }

        if (!choices.empty()) {
            std::cout << "\n선택지:\n";
            for (const auto& choice : choices) {
                std::cout << "  " << choice << '\n';
            }
            std::cout << "번호를 고르거나 직접 문장으로 적으세요. (종료: q)\n";
            return;
        }

        std::cout << "\n직접 문장으로 적으세요. (종료: q)\n";
    }

    std::string TextRpgApp::executeTurn(const std::string& input, bool interactive, bool& gameEnded)
    {
        gameEnded = false;
        const auto turnInput = trimDisplayText(input);
        if (turnInput.empty()) {
            return "문장을 적고 Enter를 눌러 주세요.";
        }

        const bool wasCombatActive = combat_.isActive();
        const auto playerAction = parsePlayerAction(turnInput);
        std::string actionContext;
        std::ostringstream turnResult;
        StatChanges visibleChanges;

        if (playerAction.showSkillMenu) {
            waitingForSkillSelection_ = true; // 다음 입력을 스킬 번호로 받기 위해 상태 켜기
            std::ostringstream out;
            out << "사용할 스킬 번호를 고르세요:\n";
            const auto skillNames = combat_.getPlayerSkillNames();
            for (std::size_t i = 0; i < skillNames.size(); ++i) {
                out << "  " << i + 1 << ". " << skillNames[i] << '\n';
            }
            state_.world.decisionHint = out.str(); // 화면 힌트에 스킬 목록 업데이트

            if (interactive) {
                std::ostringstream uiOut;
                appendUiBlock(uiOut, "스킬 선택", out.str());
                return uiOut.str(); // 이번 턴 조기 종료 (적 행동 안 함)
            }
            return out.str();
        }

        if (playerAction.isBaseUnlock) {
            unlockBase();
            saveRecordsJson();
            const auto body = "이곳이 거점이 되었습니다.\n" + state_.records.base.location;
            if (!interactive) {
                std::cout << "\n" << body << '\n';
            }
            return body;
        }

        if (playerAction.isElderDialogue) {
            npcManager_.updateAffinity("장로", 10);
            const auto result = llm_.generateElderDialogue(state_);
            saveRecordsJson();
            if (!interactive) {
                printElderDialogue(result);
            }
            saveRecordsJson();
            return formatElderDialogueForUi(result);
        }

        if (playerAction.baseService.has_value()) {
            const auto body = resolveBaseService(playerAction.baseService.value());
            saveRecordsJson();
            if (!interactive) {
                std::cout << "\n" << body << '\n';
                return body;
            }
            std::ostringstream out;
            appendUiBlock(out, "거점", body);
            return out.str();
        }

        if (playerAction.isCustom) {
            const auto [diceValue, outcome] = rollD12();
            if (!interactive) {
                std::cout << "\n" << formatDiceRollForUi(diceValue, outcome) << '\n';
            }

            std::ostringstream diceOut;
            diceOut << "d12: " << diceValue << " -> " << diceOutcomeToKorean(outcome);

            if (wasCombatActive) {
                SoundManager::instance().playAttack();
                const auto result = combat_.resolveCustomAction(state_, playerAction.customText, outcome, diceValue);
                const auto displayText = result.logText.empty() ? result.actionContext : result.logText;
                if (!combat_.isActive() && state_.player.hp > 0) {
                    SoundManager::instance().playVictory();
                }
                state_.world.decisionHint = combat_.isActive()
                    ? "전투가 계속됩니다. 다음 행동을 고르세요."
                    : (state_.player.hp <= 0
                        ? "전투에서 쓰러졌습니다."
                        : "전투가 끝났습니다. 숨을 고르고 다음 행동을 정하세요.");
                ++state_.turnNumber;
                saveRecordsJson();
                gameEnded = state_.player.hp <= 0;

                if (interactive) {
                    appendUiBlock(turnResult, "판정", formatDiceRollForUi(diceValue, outcome));
                    appendUiBlock(turnResult, "상황", displayText);
                    return turnResult.str();
                }

                std::cout << "\n" << displayText << '\n';
                return displayText;
            }

            const auto actionResult = llm_.generateActionResult(state_, playerAction.customText, diceOut.str());
            visibleChanges = addStatChanges(visibleChanges, actionResult.statChanges);
            saveRecordsJson();
            if (interactive) {
                if (actionResult.item.has_value()) {
                    SoundManager::instance().playItemGain();
                }
                appendUiBlock(turnResult, "판정", formatDiceRollForUi(diceValue, outcome));
                appendUiBlock(turnResult, "행동", formatActionResultForUi(actionResult));
                appendUiBlock(turnResult, "획득", formatItemGain(actionResult.item));
            }
            else {
                printActionResult(actionResult);
            }
            maybePromptBaseUnlock(playerAction.customText);
            saveRecordsJson();

            std::ostringstream out;
            out << "고유 행동: " << playerAction.customText << '\n';
            out << diceOut.str() << '\n';
            out << "판정 규칙: 1~5 실패, 6~10 성공, 11~12 초대박.\n";
            out << "고유 행동 직접 결과: " << actionResult.resultText << '\n';
            out << "다음 이벤트 힌트: " << actionResult.nextEventHint << '\n';
            out << "이번 다음 이벤트에서 고유 행동 결과 이후의 장면을 선택한다.";
            actionContext = out.str();
        }
        else {
            actionContext = resolveDefaultAction(playerAction);
            if (wasCombatActive) {
                const auto displayText = cleanDisplayText(actionContext);
                if (playerAction.combatChoice.has_value()) {
                    if (!combat_.isActive() && state_.player.hp > 0) {
                        SoundManager::instance().playVictory();
                    }
                    state_.world.decisionHint = combat_.isActive()
                        ? "전투가 계속됩니다. 다음 행동을 고르세요."
                        : (state_.player.hp <= 0
                            ? "전투에서 쓰러졌습니다."
                            : "전투가 끝났습니다. 숨을 고르고 다음 행동을 정하세요.");
                    ++state_.turnNumber;
                }
                else if (!displayText.empty()) {
                    state_.world.decisionHint = displayText;
                }
                saveRecordsJson();
                gameEnded = state_.player.hp <= 0;
                if (!interactive && !displayText.empty()) {
                    std::cout << "\n" << displayText << '\n';
                }
                if (interactive) {
                    std::ostringstream out;
                    appendUiBlock(out, "상황", displayText);
                    return out.str();
                }
                return displayText;
            }
        }

        const auto npcPrompt = npcManager_.generateNPCPrompt(
            state_.world.location + "\n" + state_.currentScene + "\n" + turnInput + "\n" + actionContext);
        if (!npcPrompt.empty()) {
            actionContext += "\n";
            actionContext += npcPrompt;
        }

        GameEvent event;

        if (combat_.isActive()) {
            actionContext += "\n전투가 아직 이어지고 있으므로 엔진이 combat 이벤트를 유지한다.";
            event = llm_.generateCombatEvent(state_, actionContext);
        }
        else if (wasCombatActive) {
            actionContext += "\n전투가 끝났으므로 엔진이 비전투 후속 장면을 요청한다.";
            event = llm_.generateNonCombatEvent(state_, actionContext);
        }
        else {
            const auto dangerAdvance = danger_.advanceTurn(state_);
            actionContext += '\n';
            actionContext += dangerContext(dangerAdvance);
            if (dangerAdvance.combatTriggered) {
                event = llm_.generateCombatEvent(state_, actionContext);
                danger_.resetAfterCombat(state_);
            }
            else {
                event = llm_.generateNonCombatEvent(state_, actionContext);
            }
        }
        if (eventStatsAreApplied(event)) {
            visibleChanges = addStatChanges(visibleChanges, event.statChanges);
        }
        saveRecordsJson();
        if (interactive) {
            if (event.item.has_value()) {
                SoundManager::instance().playItemGain();
            }
            appendUiBlock(turnResult, "스토리", formatEventForUi(event, hasMainObjective(state_.records)));
            appendUiBlock(turnResult, "획득", formatItemGain(event.item));
            const auto statText = formatStatChanges(visibleChanges);
            if (!statText.empty()) {
                appendUiBlock(turnResult, "변화", statText);
            }
        }
        else {
            printEvent(event);
            const auto statText = formatStatChanges(visibleChanges);
            if (!statText.empty()) {
                std::cout << "\n" << statText << '\n';
            }
        }

        combat_.updateFromEvent(event);
        gameEnded = event.eventType == ids::event::GameEnd;

        saveRecordsJson();
        return turnResult.str();
    }

    ActionInputInfo TextRpgApp::makeActionInputInfo() const
    {
        ActionInputInfo info;
        info.turnNumber = state_.turnNumber;
        info.combatActive = combat_.isActive();
        info.waitingForSkillSelection = waitingForSkillSelection_;
        info.canTalkToElder = canTalkToElder();
        info.canSetBase = canSetBaseHere();
        info.canUseBaseServices = canUseBaseServices();
        info.hasMainObjective = hasMainObjective(state_.records);
        info.location = state_.world.location;
        info.prologue = state_.records.prologue.text;
        info.personalGoal = state_.records.prologue.personalGoal;
        info.objective = state_.world.currentObjective;
        info.decisionHint = state_.world.decisionHint;
        info.sceneText = state_.currentScene;
        info.level = state_.player.level;
        info.hp = state_.player.hp;
        info.maxHp = state_.player.maxHp;
        info.attack = state_.player.attack;
        info.defense = state_.player.defense;
        info.gold = state_.player.gold;
        info.exp = state_.player.exp;
        info.danger = state_.records.danger.level;
        info.dangerThreshold = state_.records.danger.threshold;
        info.inventory = state_.player.inventory;
        info.recentEvents = state_.memory.recentEvents;
        info.chatHistory = uiChatHistory_;
        return info;
    }

    std::string TextRpgApp::readInlineInput(const std::string& prompt)
    {
        std::cout << prompt;
        if (scriptedIndex_ < config_.scriptedInputs.size()) {
            const auto input = config_.scriptedInputs[scriptedIndex_++];
            if (isAffirmative(input) || isNegative(input)) {
                std::cout << input << '\n';
                return input;
            }

            --scriptedIndex_;
            std::cout << "아니오\n";
            return "아니오";
        }

        std::string input;
        std::getline(std::cin, input);
        return input;
    }

    std::string TextRpgApp::readPlayerInput()
    {
        if (config_.scriptedInputs.empty()) {
            return view_.readActionInput(makeActionInputInfo());
        }

        printChoiceMenu();
        std::cout << "\n> ";

        std::string input;
        std::getline(std::cin, input);
        return input;
    }

    TextRpgApp::PlayerAction TextRpgApp::parsePlayerAction(std::string input) const
    {
        PlayerAction action;

        // 1. 만약 스킬 선택 대기 상태라면, 들어온 입력을 무조건 스킬 번호로 취급합니다.
        if (waitingForSkillSelection_) {
            int choiceIdx = 0;
            try {
                choiceIdx = std::stoi(input) - 1;
            }
            catch (...) {
                choiceIdx = 0;
            }

            const auto skillNames = combat_.getPlayerSkillNames();
            if (choiceIdx < 0 || choiceIdx >= static_cast<int>(skillNames.size())) {
                choiceIdx = 0;
            }

            action.combatChoice = CombatChoice::Attack;
            action.skillIndex = choiceIdx;
            action.actionContext = "플레이어는 스킬 [" + skillNames[choiceIdx] + "]을(를) 발동했다.";
            return action;
        }

        // 2. 비전투 상황의 행동 처리 (거점, 장로, 여관, 상점 등)
        if (!combat_.isActive() && containsText(input, "거점") && canSetBaseHere()) {
            action.isBaseUnlock = true;
            action.actionContext = "플레이어는 이 지역을 거점으로 삼는다.";
            return action;
        }

        if (!combat_.isActive() && containsText(input, "장로")) {
            if (canTalkToElder()) {
                action.isElderDialogue = true;
                action.actionContext = "플레이어는 거점의 장로와 대화해 보스 정보를 듣는다.";
                return action;
            }
            if (state_.records.elder.talked) {
                action.actionContext = "플레이어는 장로를 다시 찾아가려 했지만, 보스 정보에 관한 핵심 대화는 이미 끝났다.";
                return action;
            }
            if (state_.records.base.unlocked) {
                action.actionContext = "플레이어는 장로와 대화하려 했지만, 지금은 장로가 있는 거점에 있지 않다.";
                return action;
            }
        }

        if (!combat_.isActive() && canUseBaseServices()) {
            if (containsText(input, "여관") || containsText(input, "휴식")) {
                action.baseService = PlayerAction::BaseService::Inn;
                action.actionContext = "플레이어는 거점의 여관에서 휴식한다.";
                return action;
            }
            if (containsText(input, "강화")) {
                action.baseService = containsText(input, "방어") || containsText(input, "갑옷")
                    ? PlayerAction::BaseService::UpgradeArmor
                    : PlayerAction::BaseService::UpgradeWeapon;
                action.actionContext = "플레이어는 거점의 대장장이에게 장비 강화를 맡긴다.";
                return action;
            }
            if (containsText(input, "상점")
                || containsText(input, "구매")
                || containsText(input, "거래")
                || containsText(input, "물약")
                || containsText(input, "회복")
                || containsText(input, "무기")
                || containsText(input, "방어구")
                || containsText(input, "갑옷")) {
                if (containsText(input, "물약") || containsText(input, "회복") || containsText(input, "아이템")) {
                    action.baseService = PlayerAction::BaseService::BuyPotion;
                }
                else if (containsText(input, "방어구") || containsText(input, "갑옷") || containsText(input, "방어")) {
                    action.baseService = PlayerAction::BaseService::BuyArmor;
                }
                else if (containsText(input, "무기") || containsText(input, "검")) {
                    action.baseService = PlayerAction::BaseService::BuyWeapon;
                }
                else {
                    action.baseService = PlayerAction::BaseService::Shop;
                }
                action.actionContext = "플레이어는 거점의 상점 기능을 사용한다.";
                return action;
            }
        }

        // 3. 번호 메뉴 입력 처리
        if (isUnsignedInteger(input)) {
            const auto index = std::stoul(input);
            if (combat_.isActive()) {
                if (index == 1) {
                    // 1번(공격)을 눌렀을 때 콘솔 입력을 강제로 멈추지 않고, 
                    // 스킬 메뉴를 띄우라는 신호만 action 객체에 담아서 보냅니다.
                    action.showSkillMenu = true;
                    action.actionContext = "플레이어는 공격 자세를 취하며 사용할 스킬을 고민한다.";
                    return action;
                }
                if (index == 2) {
                    action.combatChoice = CombatChoice::Item;
                    action.actionContext = "플레이어는 기본 전투 선택지 [아이템]을 선택했다.";
                    return action;
                }
                action.actionContext = "전투 중에는 공격하거나 아이템을 사용할 수 있다.";
                return action;
            }
            else if (canTalkToElder()) {
                if (index == 1) {
                    action.isElderDialogue = true;
                    action.actionContext = "플레이어는 거점의 장로와 대화해 보스 정보를 듣는다.";
                    return action;
                }
            }
            else if (canSetBaseHere()) {
                if (index == 1) {
                    action.isBaseUnlock = true;
                    action.actionContext = "플레이어는 이 지역을 거점으로 삼는다.";
                    return action;
                }
            }

            std::size_t menuIndex = 1;
            if (canTalkToElder()) {
                ++menuIndex;
            }
            if (canSetBaseHere()) {
                if (index == menuIndex) {
                    action.isBaseUnlock = true;
                    action.actionContext = "플레이어는 이 지역을 거점으로 삼는다.";
                    return action;
                }
                ++menuIndex;
            }
            if (canUseBaseServices()) {
                if (index == menuIndex++) {
                    action.baseService = PlayerAction::BaseService::Shop;
                    action.actionContext = "플레이어는 거점 상점을 확인한다.";
                    return action;
                }
                if (index == menuIndex++) {
                    action.baseService = PlayerAction::BaseService::Inn;
                    action.actionContext = "플레이어는 거점 여관에서 휴식한다.";
                    return action;
                }
                if (index == menuIndex++) {
                    action.baseService = PlayerAction::BaseService::UpgradeWeapon;
                    action.actionContext = "플레이어는 거점 대장장이에게 무기 강화를 맡긴다.";
                    return action;
                }
            }

            action.isCustom = true;
            action.customText = "주변을 살핀다";
            return action;
        }

        // 4. 숫자가 아닌 기타 자유 텍스트 입력 처리 (고유 행동)
        action.isCustom = true;
        action.customText = std::move(input);
        return action;
    }

    std::string TextRpgApp::resolveDefaultAction(const PlayerAction& action)
    {
        if (action.combatChoice.has_value()) {
            if (*action.combatChoice == CombatChoice::Attack) {
                SoundManager::instance().playAttack();
                //단일공격 대신 사전에 입력 보관한 skillIndex를 Resolver로 전달
                waitingForSkillSelection_ = false; // 스킬을 골랐으므로 대기 상태 해제
                const auto result = combat_.resolvePlayerTurn(state_, action.skillIndex);
                return result.logText.empty() ? result.actionContext : result.logText;
            }
            if (*action.combatChoice == CombatChoice::Item) {
                const auto result = combat_.resolveItemUse(state_);
                return result.logText.empty() ? result.actionContext : result.logText;
            }
            return action.actionContext;
        }

        return action.actionContext;
    }

    std::string TextRpgApp::resolveBaseService(PlayerAction::BaseService service)
    {
        if (!canUseBaseServices()) {
            return "지금은 거점 기능을 사용할 수 없다.";
        }

        const auto buyItem = [&](const Item& item, int price) {
            std::ostringstream out;
            if (state_.player.gold < price) {
                out << "Gold가 부족하다. 필요 " << price << ", 보유 " << state_.player.gold << '.';
                return out.str();
            }

            auto inventory = textrpg::game::Inventory::fromLLMItems(state_.player.inventory);
            if (!inventory.addLLMItem(item)) {
                return std::string("가방이 가득 차서 더 살 수 없다.");
            }

            const int beforeGold = state_.player.gold;
            const int beforeAttack = state_.player.attack;
            const int beforeDefense = state_.player.defense;
            state_.player.gold -= price;
            state_.player.inventory = inventory.toLLMItems();
            if (item.type == ids::item::Weapon) {
                state_.player.attack += std::max(1, item.value / 10);
            }
            else if (item.type == ids::item::Armor) {
                state_.player.defense += std::max(1, item.value / 10);
            }
            rememberObtainedItem(state_, item);
            npcManager_.updateAffinity("상인", 1);
            pushLimited(state_.memory.recentEvents, "상점 구매: " + item.name, 5);

            out << "상인이 물건을 건넸다.\n"
                << summarizeGameItem(item) << '\n'
                << "Gold " << beforeGold << " -> " << state_.player.gold;
            if (state_.player.attack != beforeAttack) {
                out << "\nATK " << beforeAttack << " -> " << state_.player.attack;
            }
            if (state_.player.defense != beforeDefense) {
                out << "\nDEF " << beforeDefense << " -> " << state_.player.defense;
            }
            return out.str();
            };

        switch (service) {
        case PlayerAction::BaseService::Shop: {
            std::ostringstream out;
            out << "상점에서 살 수 있는 물건\n";
            out << "물약 구매: 10G - " << summarizeGameItem(shopPotion()) << '\n';
            out << "무기 구매: 18G - " << summarizeGameItem(shopWeapon()) << '\n';
            out << "방어구 구매: 18G - " << summarizeGameItem(shopArmor());
            npcManager_.updateAffinity("상인", 1);
            return out.str();
        }
        case PlayerAction::BaseService::BuyPotion:
            return buyItem(shopPotion(), 10);
        case PlayerAction::BaseService::BuyWeapon:
            return buyItem(shopWeapon(), 18);
        case PlayerAction::BaseService::BuyArmor:
            return buyItem(shopArmor(), 18);
        case PlayerAction::BaseService::Inn: {
            constexpr int cost = 8;
            if (state_.player.hp >= state_.player.maxHp) {
                return "여관주인은 몸 상태를 살피더니, 지금은 쉴 필요가 없다고 말했다.";
            }
            if (state_.player.gold < cost) {
                std::ostringstream out;
                out << "여관비가 부족하다. 필요 " << cost << ", 보유 " << state_.player.gold << '.';
                return out.str();
            }

            const int beforeHp = state_.player.hp;
            const int beforeGold = state_.player.gold;
            state_.player.gold -= cost;
            state_.player.hp = state_.player.maxHp;
            npcManager_.updateAffinity("여관주인", 1);
            pushLimited(state_.memory.recentEvents, "여관 휴식", 5);

            std::ostringstream out;
            out << "여관에서 몸을 추슬렀다.\n"
                << "HP " << beforeHp << " -> " << state_.player.hp << '\n'
                << "Gold " << beforeGold << " -> " << state_.player.gold;
            return out.str();
        }
        case PlayerAction::BaseService::UpgradeWeapon:
        case PlayerAction::BaseService::UpgradeArmor: {
            const bool weapon = service == PlayerAction::BaseService::UpgradeWeapon;
            constexpr int cost = 12;
            if (state_.player.gold < cost) {
                std::ostringstream out;
                out << "강화 비용이 부족하다. 필요 " << cost << ", 보유 " << state_.player.gold << '.';
                return out.str();
            }

            const int beforeGold = state_.player.gold;
            state_.player.gold -= cost;
            if (weapon) {
                const int beforeAttack = state_.player.attack;
                ++state_.player.attack;
                npcManager_.updateAffinity("대장장이", 1);
                pushLimited(state_.memory.recentEvents, "무기 강화", 5);
                std::ostringstream out;
                out << "대장장이가 무기의 균형을 바로잡았다.\n"
                    << "ATK " << beforeAttack << " -> " << state_.player.attack << '\n'
                    << "Gold " << beforeGold << " -> " << state_.player.gold;
                return out.str();
            }

            const int beforeDefense = state_.player.defense;
            ++state_.player.defense;
            npcManager_.updateAffinity("대장장이", 1);
            pushLimited(state_.memory.recentEvents, "방어구 강화", 5);
            std::ostringstream out;
            out << "대장장이가 방어구의 약한 곳을 덧댔다.\n"
                << "DEF " << beforeDefense << " -> " << state_.player.defense << '\n'
                << "Gold " << beforeGold << " -> " << state_.player.gold;
            return out.str();
        }
        }

        return "거점 기능을 사용할 수 없다.";
    }

    bool TextRpgApp::canTalkToElder() const
    {
        if (!state_.records.base.unlocked || state_.records.elder.talked) {
            return false;
        }
        if (state_.records.base.location.empty()) {
            return false;
        }
        return state_.world.location == state_.records.base.location;
    }

    bool TextRpgApp::canSetBaseHere() const
    {
        return !combat_.isActive()
            && !state_.records.base.unlocked
            && state_.world.baseCandidate
            && !state_.world.location.empty()
            && !wasBaseDeclinedAtLocation();
    }

    bool TextRpgApp::canUseBaseServices() const
    {
        return !combat_.isActive()
            && state_.records.base.unlocked
            && !state_.records.base.location.empty()
            && state_.world.location == state_.records.base.location;
    }

    bool TextRpgApp::maybePromptBaseUnlock(const std::string& triggerText)
    {
        if (state_.records.base.unlocked) {
            return false;
        }

        const bool explicitBaseAction = containsText(triggerText, "거점");
        if (!canSetBaseHere()) {
            return false;
        }

        if (config_.scriptedInputs.empty()) {
            if (!explicitBaseAction) {
                return false;
            }
            unlockBase();
            std::cout << "\n거점 기능이 해방되었습니다: " << state_.records.base.location << '\n';
            return true;
        }

        const auto answer = readInlineInput("\n이 지역을 거점으로 삼으시겠습니까? (예/아니오): ");
        if (isAffirmative(answer)) {
            unlockBase();
            std::cout << "거점 기능이 해방되었습니다: " << state_.records.base.location << '\n';
            return true;
        }

        if (isNegative(answer) || !answer.empty()) {
            markBaseDeclined();
            std::cout << "거점 지정을 보류했습니다.\n";
        }
        return false;
    }

    bool TextRpgApp::wasBaseDeclinedAtLocation() const
    {
        return std::find(
            state_.records.base.declinedLocations.begin(),
            state_.records.base.declinedLocations.end(),
            state_.world.location) != state_.records.base.declinedLocations.end();
    }

    void TextRpgApp::markBaseDeclined()
    {
        if (!state_.world.location.empty() && !wasBaseDeclinedAtLocation()) {
            state_.records.base.declinedLocations.push_back(state_.world.location);
        }
    }

    void TextRpgApp::unlockBase()
    {
        state_.records.base.unlocked = true;
        state_.records.base.location = state_.world.location.empty() ? "이름 없는 거점" : state_.world.location;
        state_.world.baseCandidate = false;
        state_.records.elder.introduced = true;
        state_.records.base.features = {
            "무기 거래",
            "무기 강화",
            "여관",
            "아이템 구매",
        };
        pushLimited(state_.memory.recentEvents, "거점 해방: " + state_.records.base.location, 5);
    }

    bool TextRpgApp::scriptedInputFinished() const
    {
        return !config_.scriptedInputs.empty() && scriptedIndex_ >= config_.scriptedInputs.size();
    }

} // namespace textrpg::app