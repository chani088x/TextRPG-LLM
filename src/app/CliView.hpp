#pragma once

#include "llm/LLMTypes.hpp"

#include <functional>
#include <string>
#include <vector>

namespace textrpg::app {

enum class StartMenuChoice {
    NewGame,
    ContinueGame,
    Exit,
};

struct ActionInputInfo {
    int turnNumber = 0;
    bool combatActive = false;
    bool waitingForSkillSelection = false;
    bool canTalkToElder = false;
    bool canSetBase = false;
    bool canUseBaseServices = false;
    bool hasMainObjective = false;
    std::string location;
    std::string prologue;
    std::string personalGoal;
    std::string objective;
    std::string decisionHint;
    std::string sceneText;
    int level = 1;
    int hp = 0;
    int maxHp = 1;
    int attack = 0;
    int defense = 0;
    int gold = 0;
    int exp = 0;
    int danger = 0;
    int dangerThreshold = 10;
    std::vector<llm::Item> inventory;
    std::vector<std::string> recentEvents;
    std::vector<std::string> chatHistory;
};

struct ResultViewInfo {
    ActionInputInfo status;
    std::string title;
    std::string body;
};

struct TurnViewResult {
    ActionInputInfo status;
    std::string body;
    bool exitRequested = false;
    bool skipReveal = false;
};

class CliView {
public:
    StartMenuChoice showStartMenu(bool hasSave) const;
    void showLoadingScreen(const std::string& message) const;
    void runWithLoadingScreen(const std::string& message, const std::function<void()>& task) const;
    void showResultScreen(const ResultViewInfo& info) const;
    std::string readActionInput(const ActionInputInfo& info) const;
    TurnViewResult runActionTurn(
        const ActionInputInfo& info,
        const std::function<TurnViewResult(const std::string&)>& processor) const;

private:
    static std::string gaugeText(int value, int maxValue, int width);
};

} // namespace textrpg::app
