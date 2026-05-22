#include "app/CliView.hpp"

#include "game/Inventory.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace textrpg::app {
namespace {

constexpr int kMinFrameWidth = 80;
constexpr int kMinFrameHeight = 24;
constexpr int kMinThreeColumnWidth = 96;
constexpr int kGaugeWidth = 14;
constexpr int kAnimationDelayMs = 220;

int clampInt(int value, int minValue, int maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

bool colorEnabled()
{
    return std::getenv("NO_COLOR") == nullptr;
}

ftxui::Element tint(ftxui::Element element, ftxui::Color color)
{
    if (!colorEnabled()) {
        return element;
    }
    return element | ftxui::color(color);
}

ftxui::Element strong(ftxui::Element element)
{
    if (!colorEnabled()) {
        return element;
    }
    return element | ftxui::bold;
}

ftxui::Element selected(ftxui::Element element)
{
    if (!colorEnabled()) {
        return element;
    }
    return element | ftxui::inverted;
}

std::vector<std::string> loadLines(const std::string& path)
{
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> fallbackArt()
{
    return {
        R"(██╗     ██╗     ███╗   ███╗)",
        R"(██║     ██║     ████╗ ████║)",
        R"(██║     ██║     ██╔████╔██║)",
        R"(██║     ██║     ██║╚██╔╝██║)",
        R"(███████╗███████╗██║ ╚═╝ ██║)",
        R"(╚══════╝╚══════╝╚═╝     ╚═╝)",
    };
}

std::vector<std::string> startArt()
{
    auto art = loadLines("data/start_art_4x3.txt");
    if (art.empty()) {
        art = fallbackArt();
    }
    return art;
}

ftxui::Element artBlock(const std::vector<std::string>& lines)
{
    std::vector<ftxui::Element> rows;
    rows.reserve(lines.size());
    for (const auto& line : lines) {
        rows.push_back(tint(ftxui::text(line), ftxui::Color::GrayLight));
    }
    return ftxui::vbox(std::move(rows));
}

std::string spinnerFrame(int frame)
{
    static const char frames[] = {'|', '/', '-', '\\'};
    return std::string(1, frames[frame % 4]);
}

std::string activityDots(int frame)
{
    static const char* frames[] = {"   ", ".  ", ".. ", "..."};
    return frames[frame % 4];
}

enum class TurnScreenMode {
    Input,
    Loading,
    Revealing,
};

std::string trimText(std::string value);

std::vector<std::vector<std::string>> splitRevealBlocks(const std::string& text)
{
    std::vector<std::vector<std::string>> blocks;
    std::vector<std::string> currentBlock;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        line = trimText(line);
        if (line.empty()) {
            if (!currentBlock.empty()) {
                blocks.push_back(currentBlock);
                currentBlock.clear();
            }
            continue;
        }
        currentBlock.push_back(line);
    }
    if (!currentBlock.empty()) {
        blocks.push_back(currentBlock);
    }
    if (blocks.empty()) {
        blocks.push_back({"표시할 내용이 없습니다."});
    }
    return blocks;
}

std::thread startAnimationTicker(ftxui::ScreenInteractive& screen, std::atomic_bool& running)
{
    return std::thread([&screen, &running] {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kAnimationDelayMs));
            screen.PostEvent(ftxui::Event::Custom);
        }
    });
}

std::string makeGaugeText(int value, int maxValue, int width)
{
    if (maxValue <= 0) {
        maxValue = 1;
    }
    value = clampInt(value, 0, maxValue);
    const int filled = (value * width + maxValue - 1) / maxValue;

    std::string result = "[";
    result.append(static_cast<std::size_t>(filled), '#');
    result.append(static_cast<std::size_t>(width - filled), '-');
    result += "]";
    return result;
}

ftxui::Element fullFrame(ftxui::Element body)
{
    return body
        | ftxui::border
        | ftxui::flex;
}

ftxui::Element menuLine(const std::string& label, bool isSelected, bool disabled = false)
{
    auto line = ftxui::text((isSelected ? "> " : "  ") + label)
        | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 28);
    if (disabled) {
        return tint(line, ftxui::Color::GrayDark);
    }
    return isSelected ? selected(strong(line)) : line;
}

ftxui::Element startMenuDocument(
    const std::vector<std::string>& art,
    bool hasSave,
    int selectedIndex,
    int width,
    int height,
    int frame)
{
    if (width < kMinFrameWidth || height < kMinFrameHeight) {
        return fullFrame(ftxui::vbox({
            ftxui::filler(),
            strong(ftxui::text("LLM TEXT RPG")) | ftxui::center,
            tint(ftxui::text("터미널 창을 조금 더 크게 키워 주세요."), ftxui::Color::GrayLight) | ftxui::center,
            tint(ftxui::text("최소 80x24"), ftxui::Color::GrayLight) | ftxui::center,
            ftxui::filler(),
        }));
    }

    const int menuWidth = clampInt(width / 3, 34, 44);
    auto artPanel = ftxui::vbox({
        ftxui::filler(),
        artBlock(art) | ftxui::center,
        ftxui::filler(),
    }) | ftxui::border
       | ftxui::flex;

    auto menuPanel = ftxui::vbox({
        ftxui::filler(),
        strong(ftxui::text("LLM TEXT RPG")) | ftxui::center,
        ftxui::separatorEmpty(),
        menuLine("새 게임", selectedIndex == 0),
        menuLine(hasSave ? "이어하기" : "이어하기 (기록 없음)", selectedIndex == 1, !hasSave),
        menuLine("종료", selectedIndex == 2),
        ftxui::separatorEmpty(),
        tint(ftxui::text(spinnerFrame(frame) + " 신호 대기 중" + activityDots(frame)), ftxui::Color::GrayLight) | ftxui::center,
        tint(ftxui::text("[↑/↓] 선택   [Enter] 실행   [q] 종료"), ftxui::Color::GrayLight),
        ftxui::filler(),
    }) | ftxui::border
       | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, menuWidth)
       | ftxui::yflex;

    return fullFrame(ftxui::hbox({artPanel, menuPanel}));
}

ftxui::Element loadingDocument(const std::vector<std::string>& art, const std::string& message, int frame)
{
    std::vector<ftxui::Element> rows = {
        ftxui::filler(),
        artBlock(art) | ftxui::center,
        ftxui::separatorEmpty(),
        strong(ftxui::text(spinnerFrame(frame) + " 이야기를 준비하고 있습니다" + activityDots(frame))) | ftxui::center,
        tint(ftxui::paragraph(message), ftxui::Color::GrayLight) | ftxui::center,
        ftxui::separatorEmpty(),
        tint(ftxui::text("잠시만 기다려 주세요."), ftxui::Color::GrayLight) | ftxui::center,
        ftxui::filler(),
    };

    return fullFrame(ftxui::vbox(std::move(rows)));
}

ftxui::Element metricLine(const std::string& label, int value, int maxValue, ftxui::Color color)
{
    std::ostringstream valueText;
    valueText << value << "/" << maxValue;
    return ftxui::hbox({
        ftxui::text(label) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
        tint(ftxui::text(makeGaugeText(value, maxValue, kGaugeWidth)), color),
        ftxui::text(" " + valueText.str()),
    });
}

std::vector<ftxui::Element> choiceRows(const ActionInputInfo& info)
{
    std::vector<std::string> choices;
    if (info.combatActive) {
        choices = {"1. 공격", "2. 아이템"};
    } else {
        int index = 1;
        if (info.canTalkToElder) {
            choices.push_back(std::to_string(index++) + ". 장로와 대화");
        }
        if (info.canSetBase) {
            choices.push_back(std::to_string(index++) + ". 거점으로 삼기");
        }
        if (info.canUseBaseServices) {
            choices.push_back(std::to_string(index++) + ". 상점 보기");
            choices.push_back(std::to_string(index++) + ". 여관에서 휴식");
            choices.push_back(std::to_string(index++) + ". 장비 강화");
        }
    }

    std::vector<ftxui::Element> rows;
    if (choices.empty()) {
        return rows;
    }
    rows.push_back(tint(ftxui::text("선택지"), ftxui::Color::GrayLight));
    for (const auto& choice : choices) {
        rows.push_back(ftxui::text("  " + choice));
    }
    return rows;
}

struct RecentEntry {
    std::string label;
    std::string body;
};

std::string trimText(std::string value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

RecentEntry parseRecentEntry(const std::string& raw)
{
    const auto split = raw.find(':');
    if (split == std::string::npos) {
        return {"", trimText(raw)};
    }

    auto label = trimText(raw.substr(0, split));
    auto body = trimText(raw.substr(split + 1));
    if (label.rfind("턴 ", 0) == 0) {
        label.clear();
    }

    if (label == "시작 상황") {
        label.clear();
    } else if (label == "고유 행동 결과") {
        label.clear();
    } else if (label == "전투 결과") {
        label.clear();
    } else if (label == "아이템 사용") {
        label.clear();
    } else if (label == "거점 해방") {
        body = "거점 기능이 해방되었습니다. " + body;
        label.clear();
    } else if (label == "장로 대화") {
        label.clear();
    }

    return {label, body.empty() ? raw : body};
}

std::vector<ftxui::Element> recentRows(const std::vector<std::string>& recentEvents, std::size_t maxCount = 3)
{
    std::vector<ftxui::Element> rows;
    rows.push_back(tint(ftxui::text("최근 기록"), ftxui::Color::GrayLight));
    std::vector<std::string> visibleEvents;
    visibleEvents.reserve(recentEvents.size());
    for (const auto& event : recentEvents) {
        if (event.rfind("프롤로그:", 0) != 0 && event.rfind("시작 상황:", 0) != 0) {
            visibleEvents.push_back(event);
        }
    }

    if (visibleEvents.empty()) {
        rows.push_back(tint(ftxui::text("  아직 기록된 사건이 없습니다."), ftxui::Color::GrayLight));
        return rows;
    }

    const auto start = visibleEvents.size() > maxCount ? visibleEvents.size() - maxCount : 0;
    for (std::size_t i = start; i < visibleEvents.size(); ++i) {
        const auto entry = parseRecentEntry(visibleEvents[i]);
        if (i != start) {
            rows.push_back(ftxui::separatorEmpty());
        }
        rows.push_back(ftxui::hbox({
            tint(ftxui::text("  - "), ftxui::Color::GrayLight),
            ftxui::paragraph(entry.body) | ftxui::flex,
        }));
    }
    return rows;
}

std::vector<ftxui::Element> inventoryRows(const std::vector<llm::Item>& inventory)
{
    std::vector<ftxui::Element> rows;
    rows.push_back(strong(ftxui::text("아이템")));
    if (inventory.empty()) {
        rows.push_back(tint(ftxui::text("  소지품 없음"), ftxui::Color::GrayLight));
        return rows;
    }

    const auto gameInventory = game::Inventory::fromLLMItems(inventory);
    const auto summaries = gameInventory.summaries(8);
    for (const auto& summary : summaries) {
        rows.push_back(ftxui::paragraph("  " + summary));
    }
    if (static_cast<std::size_t>(gameInventory.size()) > summaries.size()) {
        const auto hiddenCount = static_cast<std::size_t>(gameInventory.size()) - summaries.size();
        rows.push_back(tint(ftxui::text("  +" + std::to_string(hiddenCount) + "개 더"), ftxui::Color::GrayLight));
    }
    return rows;
}

ftxui::Element logoPanel(const std::vector<std::string>& art, const ActionInputInfo& info, int frame)
{
    const auto story = !info.sceneText.empty()
        ? info.sceneText
        : (!info.prologue.empty() ? info.prologue : std::string("아직 표시할 장면이 없습니다."));
    const auto direction = info.personalGoal.empty() ? info.objective : info.personalGoal;
    const auto objectiveText = info.objective.empty() ? std::string("장로에게 들은 정보를 확인하세요.") : info.objective;

    std::vector<ftxui::Element> rows = {
        artBlock(art) | ftxui::center,
        tint(ftxui::text(spinnerFrame(frame) + activityDots(frame)), ftxui::Color::GrayLight) | ftxui::center,
        ftxui::separator(),
        strong(ftxui::text("스토리")),
        ftxui::paragraph(story),
        ftxui::separator(),
        strong(ftxui::text(info.hasMainObjective ? "목표" : "동기")),
        ftxui::paragraph(info.hasMainObjective ? objectiveText : direction),
        ftxui::separator(),
    };

    auto recent = recentRows(info.recentEvents, 4);
    rows.insert(rows.end(), recent.begin(), recent.end());
    rows.push_back(ftxui::filler());

    return ftxui::vbox(std::move(rows))
        | ftxui::border
        | ftxui::flex;
}

struct TurnRenderState {
    TurnScreenMode mode = TurnScreenMode::Input;
    std::vector<std::string> revealedLines;
    int scrollFocus = 0;
    bool followBottom = true;
    std::string notice;
};

void appendChatHistoryRows(std::vector<ftxui::Element>& rows, const std::vector<std::string>& chatHistory)
{
    if (chatHistory.empty()) {
        return;
    }

    rows.push_back(tint(ftxui::text("대화"), ftxui::Color::GrayLight));
    const auto start = chatHistory.size() > 8 ? chatHistory.size() - 8 : 0;
    for (std::size_t i = start; i < chatHistory.size(); ++i) {
        if (i != start) {
            rows.push_back(ftxui::separatorEmpty());
        }
        rows.push_back(ftxui::hbox({
            ftxui::text("  "),
            ftxui::paragraph(chatHistory[i]) | ftxui::flex,
        }));
    }
    rows.push_back(ftxui::separator());
}

ftxui::Element scrollableLog(std::vector<ftxui::Element> rows, int focusIndex)
{
    if (rows.empty()) {
        rows.push_back(ftxui::text(""));
    }

    const int selectedIndex = clampInt(focusIndex, 0, static_cast<int>(rows.size()) - 1);
    rows[static_cast<std::size_t>(selectedIndex)] = ftxui::focus(rows[static_cast<std::size_t>(selectedIndex)]);
    return ftxui::vbox(std::move(rows))
        | ftxui::yframe
        | ftxui::vscroll_indicator
        | ftxui::flex;
}

ftxui::Element chatPanel(const ActionInputInfo& info, ftxui::Element inputElement, int frame)
{
    const auto mode = info.combatActive ? "전투" : "탐험";
    auto status = ftxui::hbox({
        strong(ftxui::text(mode)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
        ftxui::text("턴 " + std::to_string(info.turnNumber)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
        ftxui::paragraph(info.location) | ftxui::flex,
        tint(ftxui::text(spinnerFrame(frame)), ftxui::Color::GrayLight),
    });

    std::vector<ftxui::Element> logRows;
    appendChatHistoryRows(logRows, info.chatHistory);
    logRows.push_back(tint(ftxui::paragraph(info.decisionHint), ftxui::Color::GrayLight));
    auto choices = choiceRows(info);
    logRows.insert(logRows.end(), choices.begin(), choices.end());

    auto logPanel = scrollableLog(std::move(logRows), 1000000)
        | ftxui::border
        | ftxui::flex;

    auto inputPanel = ftxui::hbox({
        tint(ftxui::text("> "), ftxui::Color::GrayLight),
        inputElement | ftxui::flex,
    }) | ftxui::border
       | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3);

    return ftxui::vbox({
        status,
        ftxui::separator(),
        logPanel,
        inputPanel,
        tint(ftxui::text("[Enter] 전송   [Esc] 종료"), ftxui::Color::GrayLight),
    }) | ftxui::border
       | ftxui::flex;
}

ftxui::Element turnChatPanel(
    const ActionInputInfo& info,
    ftxui::Element inputElement,
    const TurnRenderState& turnState,
    int frame)
{
    const auto mode = info.combatActive ? "전투" : "탐험";
    auto status = ftxui::hbox({
        strong(ftxui::text(mode)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
        ftxui::text("턴 " + std::to_string(info.turnNumber)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10),
        ftxui::paragraph(info.location) | ftxui::flex,
        tint(ftxui::text(spinnerFrame(frame)), ftxui::Color::GrayLight),
    });

    std::vector<ftxui::Element> logRows;
    appendChatHistoryRows(logRows, info.chatHistory);

    if (turnState.mode == TurnScreenMode::Revealing) {
        for (const auto& line : turnState.revealedLines) {
            logRows.push_back(ftxui::paragraph(line));
        }
        if (turnState.revealedLines.empty()) {
            logRows.push_back(tint(ftxui::text("Enter를 누르면 이어집니다."), ftxui::Color::GrayLight));
        }
    } else {
        logRows.push_back(tint(ftxui::paragraph(info.decisionHint), ftxui::Color::GrayLight));
        auto choices = choiceRows(info);
        logRows.insert(logRows.end(), choices.begin(), choices.end());
        if (turnState.mode == TurnScreenMode::Loading) {
            logRows.push_back(ftxui::separator());
            logRows.push_back(strong(ftxui::text(spinnerFrame(frame) + " 처리 중" + activityDots(frame))));
        }
    }

    const int focusIndex = turnState.followBottom ? 1000000 : turnState.scrollFocus;
    auto logPanel = scrollableLog(std::move(logRows), focusIndex)
        | ftxui::border
        | ftxui::flex;

    ftxui::Element bottom;
    if (turnState.mode == TurnScreenMode::Input) {
        bottom = ftxui::vbox({
            ftxui::hbox({
                tint(ftxui::text("> "), ftxui::Color::GrayLight),
                inputElement | ftxui::flex,
            }),
            tint(ftxui::text(turnState.notice.empty() ? " " : turnState.notice), ftxui::Color::GrayLight),
        }) | ftxui::border
           | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 4);
    } else if (turnState.mode == TurnScreenMode::Loading) {
        bottom = tint(ftxui::text(spinnerFrame(frame) + " 응답을 기다리는 중" + activityDots(frame)), ftxui::Color::GrayLight)
            | ftxui::border
            | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3);
    } else {
        bottom = tint(ftxui::text("[Enter] 다음   [휠/↑↓] 스크롤"), ftxui::Color::GrayLight)
            | ftxui::border
            | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3);
    }

    return ftxui::vbox({
        status,
        ftxui::separator(),
        logPanel,
        bottom,
    }) | ftxui::border
       | ftxui::flex;
}

ftxui::Element resultPanel(const ResultViewInfo& info, int frame)
{
    std::vector<ftxui::Element> rows = {
        ftxui::hbox({
            strong(ftxui::text(info.title.empty() ? "결과" : info.title)) | ftxui::flex,
            tint(ftxui::text(spinnerFrame(frame)), ftxui::Color::GrayLight),
        }),
        ftxui::separator(),
        ftxui::paragraph(info.body.empty() ? "표시할 결과가 없습니다." : info.body) | ftxui::flex,
        ftxui::separator(),
        tint(ftxui::text("[Enter] 계속   [Esc] 닫기"), ftxui::Color::GrayLight),
    };

    return ftxui::vbox(std::move(rows))
        | ftxui::border
        | ftxui::flex;
}

ftxui::Element statusPanel(const ActionInputInfo& info, int frame)
{
    std::vector<ftxui::Element> rows = {
        strong(ftxui::text("상태 " + spinnerFrame(frame))),
        metricLine("HP", info.hp, info.maxHp, ftxui::Color::Green),
        metricLine("위험도", info.danger, info.dangerThreshold, ftxui::Color::Yellow),
        ftxui::separator(),
        ftxui::text("LV   " + std::to_string(info.level)),
        ftxui::text("ATK  " + std::to_string(info.attack)),
        ftxui::text("DEF  " + std::to_string(info.defense)),
        ftxui::text("Gold " + std::to_string(info.gold)),
        ftxui::text("Exp  " + std::to_string(info.exp)),
        ftxui::separator(),
    };

    auto inventory = inventoryRows(info.inventory);
    rows.insert(rows.end(), inventory.begin(), inventory.end());

    return ftxui::vbox(std::move(rows))
        | ftxui::border
        | ftxui::flex;
}

struct PanelWidths {
    int left = 28;
    int center = 40;
    int right = 28;
};

PanelWidths panelWidthsFor(int width)
{
    const int available = std::max(kMinThreeColumnWidth - 4, width - 4);
    PanelWidths widths;
    widths.left = clampInt(available / 4, 28, 48);
    widths.right = clampInt(available / 5, 26, 38);
    widths.center = std::max(34, available - widths.left - widths.right);
    return widths;
}

ftxui::Element chatDocument(
    const ActionInputInfo& info,
    ftxui::Element inputElement,
    int width,
    int height,
    int frame)
{
    if (width < kMinThreeColumnWidth || height < kMinFrameHeight) {
        return fullFrame(ftxui::vbox({
            ftxui::filler(),
            strong(ftxui::text("LLM TEXT RPG")) | ftxui::center,
            tint(ftxui::text("3열 화면을 표시하려면 터미널 창을 조금 더 크게 키워 주세요."), ftxui::Color::GrayLight) | ftxui::center,
            tint(ftxui::text("최소 96x24"), ftxui::Color::GrayLight) | ftxui::center,
            ftxui::filler(),
        }));
    }

    const auto panelWidths = panelWidthsFor(width);
    return fullFrame(ftxui::hbox({
        logoPanel(startArt(), info, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.left) | ftxui::flex,
        chatPanel(info, std::move(inputElement), frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.center) | ftxui::flex,
        statusPanel(info, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.right) | ftxui::flex,
    }) | ftxui::flex);
}

ftxui::Element turnDocument(
    const ActionInputInfo& info,
    ftxui::Element inputElement,
    const TurnRenderState& turnState,
    int width,
    int height,
    int frame)
{
    if (width < kMinThreeColumnWidth || height < kMinFrameHeight) {
        return fullFrame(ftxui::vbox({
            ftxui::filler(),
            strong(ftxui::text("LLM TEXT RPG")) | ftxui::center,
            tint(ftxui::text("3열 화면을 표시하려면 터미널 창을 조금 더 크게 키워 주세요."), ftxui::Color::GrayLight) | ftxui::center,
            tint(ftxui::text("최소 96x24"), ftxui::Color::GrayLight) | ftxui::center,
            ftxui::filler(),
        }));
    }

    const auto panelWidths = panelWidthsFor(width);
    return fullFrame(ftxui::hbox({
        logoPanel(startArt(), info, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.left) | ftxui::flex,
        turnChatPanel(info, std::move(inputElement), turnState, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.center) | ftxui::flex,
        statusPanel(info, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.right) | ftxui::flex,
    }) | ftxui::flex);
}

ftxui::Element resultDocument(const ResultViewInfo& info, int width, int height, int frame)
{
    if (width < kMinThreeColumnWidth || height < kMinFrameHeight) {
        return fullFrame(ftxui::vbox({
            ftxui::filler(),
            strong(ftxui::text("LLM TEXT RPG")) | ftxui::center,
            tint(ftxui::text("결과 화면을 표시하려면 터미널 창을 조금 더 크게 키워 주세요."), ftxui::Color::GrayLight) | ftxui::center,
            tint(ftxui::text("최소 96x24"), ftxui::Color::GrayLight) | ftxui::center,
            ftxui::filler(),
        }));
    }

    const auto panelWidths = panelWidthsFor(width);
    return fullFrame(ftxui::hbox({
        logoPanel(startArt(), info.status, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.left) | ftxui::flex,
        resultPanel(info, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.center) | ftxui::flex,
        statusPanel(info.status, frame) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, panelWidths.right) | ftxui::flex,
    }) | ftxui::flex);
}

} // namespace

std::string CliView::gaugeText(int value, int maxValue, int width)
{
    return makeGaugeText(value, maxValue, width);
}

StartMenuChoice CliView::showStartMenu(bool hasSave) const
{
    auto art = startArt();
    auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
    StartMenuChoice result = StartMenuChoice::Exit;
    int selectedIndex = 0;
    int frame = 0;
    std::atomic_bool animating = true;
    auto ticker = startAnimationTicker(screen, animating);
    const auto moveSelection = [&](int delta) {
        do {
            selectedIndex = (selectedIndex + delta + 3) % 3;
        } while (!hasSave && selectedIndex == 1);
    };

    auto component = ftxui::Renderer([&] {
        return startMenuDocument(art, hasSave, selectedIndex, screen.dimx(), screen.dimy(), frame);
    });

    component = ftxui::CatchEvent(component, [&](const ftxui::Event& event) {
        if (event == ftxui::Event::Custom) {
            ++frame;
            return true;
        }
        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
            moveSelection(-1);
            return true;
        }
        if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
            moveSelection(1);
            return true;
        }
        if (event == ftxui::Event::Return) {
            if (selectedIndex == 0) {
                result = StartMenuChoice::NewGame;
            } else if (selectedIndex == 1 && hasSave) {
                result = StartMenuChoice::ContinueGame;
            } else {
                result = StartMenuChoice::Exit;
            }
            screen.Exit();
            return true;
        }
        if (event == ftxui::Event::Escape || event == ftxui::Event::Character('q')) {
            result = StartMenuChoice::Exit;
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    animating = false;
    if (ticker.joinable()) {
        ticker.join();
    }
    return result;
}

void CliView::showLoadingScreen(const std::string& message) const
{
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Full(), ftxui::Dimension::Full());
    auto document = loadingDocument(startArt(), message, 0);
    ftxui::Render(screen, document);
    std::cout << "\x1b[2J\x1b[H" << screen.ToString() << std::flush;
}

void CliView::runWithLoadingScreen(const std::string& message, const std::function<void()>& task) const
{
    auto art = startArt();
    auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
    std::atomic_bool done = false;
    std::atomic_bool animating = true;
    std::exception_ptr error;
    int frame = 0;

    std::thread worker([&] {
        try {
            task();
        } catch (...) {
            error = std::current_exception();
        }
        done = true;
        screen.PostEvent(ftxui::Event::Custom);
    });

    auto ticker = startAnimationTicker(screen, animating);

    auto component = ftxui::Renderer([&] {
        return loadingDocument(art, message, frame);
    });

    component = ftxui::CatchEvent(component, [&](const ftxui::Event& event) {
        if (event == ftxui::Event::Custom) {
            ++frame;
            if (done.load()) {
                screen.Exit();
            }
            return true;
        }
        return false;
    });

    screen.Loop(component);
    animating = false;
    if (ticker.joinable()) {
        ticker.join();
    }
    if (worker.joinable()) {
        worker.join();
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

void CliView::showResultScreen(const ResultViewInfo& info) const
{
    auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
    int frame = 0;
    std::atomic_bool animating = true;
    auto ticker = startAnimationTicker(screen, animating);

    auto component = ftxui::Renderer([&] {
        return resultDocument(info, screen.dimx(), screen.dimy(), frame);
    });

    component = ftxui::CatchEvent(component, [&](const ftxui::Event& event) {
        if (event == ftxui::Event::Custom) {
            ++frame;
            return true;
        }
        if (event == ftxui::Event::Return || event == ftxui::Event::Escape) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    animating = false;
    if (ticker.joinable()) {
        ticker.join();
    }
}

std::string CliView::readActionInput(const ActionInputInfo& info) const
{
    auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
    std::string input;
    int frame = 0;
    std::atomic_bool animating = true;
    auto ticker = startAnimationTicker(screen, animating);
    auto inputComponent = ftxui::Input(&input, "");

    auto component = ftxui::Renderer(inputComponent, [&] {
        return chatDocument(info, inputComponent->Render(), screen.dimx(), screen.dimy(), frame);
    });

    component = ftxui::CatchEvent(component, [&](const ftxui::Event& event) {
        if (event == ftxui::Event::Custom) {
            ++frame;
            return true;
        }
        if (event == ftxui::Event::Return) {
            screen.Exit();
            return true;
        }
        if (event == ftxui::Event::Escape && input.empty()) {
            input = "q";
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    animating = false;
    if (ticker.joinable()) {
        ticker.join();
    }
    return trimText(input);
}

TurnViewResult CliView::runActionTurn(
    const ActionInputInfo& info,
    const std::function<TurnViewResult(const std::string&)>& processor) const
{
    auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
    screen.TrackMouse(true);

    ActionInputInfo currentInfo = info;
    TurnViewResult finalResult;
    finalResult.status = info;

    std::string input;
    std::vector<std::vector<std::string>> resultBlocks;
    std::size_t revealedCount = 0;
    TurnRenderState renderState;
    int frame = 0;

    std::atomic_bool animating = true;
    std::atomic_bool workerDone = false;
    std::exception_ptr workerError;
    std::mutex resultMutex;
    std::thread worker;
    auto ticker = startAnimationTicker(screen, animating);
    auto inputComponent = ftxui::Input(&input, "");

    const auto estimatedRowCount = [&] {
        int rows = 10;
        rows += static_cast<int>(currentInfo.chatHistory.size()) * 3;
        rows += static_cast<int>(currentInfo.recentEvents.size()) * 3;
        rows += static_cast<int>(renderState.revealedLines.size());
        return std::max(rows, 1);
    };

    const auto startWorker = [&] {
        const auto submittedInput = trimText(input);
        renderState.mode = TurnScreenMode::Loading;
        renderState.followBottom = true;
        renderState.notice.clear();
        workerDone = false;
        worker = std::thread([&, submittedInput] {
            try {
                auto result = processor(submittedInput);
                std::lock_guard<std::mutex> lock(resultMutex);
                finalResult = std::move(result);
            } catch (...) {
                workerError = std::current_exception();
            }
            workerDone = true;
            screen.PostEvent(ftxui::Event::Custom);
        });
    };

    const auto prepareResultReveal = [&](const std::string& body) {
        renderState.revealedLines.clear();
        resultBlocks = splitRevealBlocks(body);
        revealedCount = 0;
        renderState.followBottom = true;
    };

    const auto revealNextBlock = [&] {
        if (revealedCount >= resultBlocks.size()) {
            screen.Exit();
            return;
        }

        const auto& block = resultBlocks[revealedCount++];
        if (!renderState.revealedLines.empty()) {
            renderState.revealedLines.push_back("");
        }
        for (const auto& line : block) {
            renderState.revealedLines.push_back(line);
        }
        renderState.followBottom = true;
    };

    const auto scrollBy = [&](int delta) {
        const int estimatedRows = estimatedRowCount();
        if (renderState.followBottom) {
            renderState.scrollFocus = std::max(0, estimatedRows - 8);
            renderState.followBottom = false;
        }
        renderState.scrollFocus = clampInt(renderState.scrollFocus + delta, 0, estimatedRows);
        if (renderState.scrollFocus >= estimatedRows - 1) {
            renderState.followBottom = true;
        }
    };

    auto component = ftxui::Renderer(inputComponent, [&] {
        return turnDocument(
            currentInfo,
            inputComponent->Render(),
            renderState,
            screen.dimx(),
            screen.dimy(),
            frame);
    });

    component = ftxui::CatchEvent(component, [&](ftxui::Event event) {
        if (event == ftxui::Event::Custom) {
            ++frame;
            if (renderState.mode == TurnScreenMode::Loading && workerDone.load()) {
                if (worker.joinable()) {
                    worker.join();
                }
                if (workerError) {
                    std::rethrow_exception(workerError);
                }
                if (finalResult.exitRequested) {
                    screen.Exit();
                    return true;
                }
                {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    currentInfo = finalResult.status;
                    prepareResultReveal(finalResult.body);
                }
                renderState.mode = TurnScreenMode::Revealing;
                renderState.followBottom = true;
            }
            return true;
        }

        if (event.is_mouse()) {
            const auto mouse = event.mouse();
            if (mouse.button == ftxui::Mouse::WheelUp) {
                scrollBy(-3);
                return true;
            }
            if (mouse.button == ftxui::Mouse::WheelDown) {
                scrollBy(3);
                return true;
            }
        }

        if (renderState.mode == TurnScreenMode::Input) {
            if (event == ftxui::Event::Return) {
                if (trimText(input).empty()) {
                    renderState.notice = "문장을 적고 Enter를 눌러 주세요.";
                    return true;
                }
                startWorker();
                return true;
            }
            if (event == ftxui::Event::Escape && input.empty()) {
                finalResult.exitRequested = true;
                screen.Exit();
                return true;
            }
            renderState.notice.clear();
            return false;
        }

        if (renderState.mode == TurnScreenMode::Loading) {
            return true;
        }

        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::PageUp) {
            scrollBy(-3);
            return true;
        }
        if (event == ftxui::Event::ArrowDown || event == ftxui::Event::PageDown) {
            scrollBy(3);
            return true;
        }
        if (event == ftxui::Event::Return) {
            revealNextBlock();
            return true;
        }
        return true;
    });

    screen.Loop(component);
    animating = false;
    if (ticker.joinable()) {
        ticker.join();
    }
    if (worker.joinable()) {
        worker.join();
    }
    if (workerError) {
        std::rethrow_exception(workerError);
    }
    return finalResult;
}

} // namespace textrpg::app
