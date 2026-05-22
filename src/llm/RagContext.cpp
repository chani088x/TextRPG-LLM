#include "llm/RagContext.hpp"

#include <RAG.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace textrpg::llm::internals {
namespace {

std::string envOr(const char* name, const std::string& fallback)
{
    if (const char* value = std::getenv(name)) {
        return value;
    }
    return fallback;
}

std::string itemText(const Item& item)
{
    std::ostringstream out;
    out << "item: " << item.name << " [" << item.type << "] value=" << item.value;
    if (!item.description.empty()) {
        out << ": " << item.description;
    }
    return out.str();
}

std::string enemyText(const Monster& monster)
{
    return "enemy: " + monster.name + (monster.description.empty() ? "" : ": " + monster.description);
}

std::vector<std::string> buildChunks(const GameState& state)
{
    std::vector<std::string> chunks;
    const auto add = [&chunks](const std::string& category, const std::string& text) {
        if (!text.empty()) {
            chunks.push_back(category + ": " + text);
        }
    };

    for (const auto& quest : state.records.questLog) {
        add("quest", quest);
    }
    if (state.records.prologue.generated) {
        add("prologue", state.records.prologue.memoryNote);
        add("protagonist_goal", state.records.prologue.personalGoal);
        add("protagonist_wound", state.records.prologue.protagonistWound);
    }
    add(
        "danger",
        std::to_string(state.records.danger.level)
            + "/" + std::to_string(state.records.danger.threshold)
            + " last+" + std::to_string(state.records.danger.lastIncrease));
    for (const auto& item : state.player.inventory) {
        chunks.push_back("inventory: " + itemText(item));
    }
    for (const auto& item : state.records.obtainedItems) {
        chunks.push_back(itemText(item));
    }
    for (const auto& enemy : state.records.encounteredEnemies) {
        chunks.push_back(enemyText(enemy));
    }
    for (const auto& memory : state.memory.recentEvents) {
        add("memory", memory);
    }
    for (const auto& choice : state.memory.importantChoices) {
        add("choice", choice);
    }
    if (state.records.base.unlocked) {
        add("base", state.records.base.location);
        for (const auto& feature : state.records.base.features) {
            add("base_feature", feature);
        }
    }
    if (state.records.elder.introduced) {
        add("elder", state.records.elder.talked ? "장로 대화 완료" : "장로 대화 가능");
    }
    if (state.records.boss.known) {
        add("boss", state.records.boss.name + " / " + state.records.boss.location + " / " + state.records.boss.weakness);
        add("boss_description", state.records.boss.description);
    }
    return chunks;
}

std::string formatChunks(const std::vector<std::string>& chunks, const std::string& note = {})
{
    if (chunks.empty()) {
        return "- 검색 가능한 기록 없음\n";
    }

    std::ostringstream out;
    const std::size_t count = std::min<std::size_t>(chunks.size(), 8);
    for (std::size_t i = 0; i < count; ++i) {
        out << "- " << chunks[i] << '\n';
    }
    if (!note.empty()) {
        out << "- rag_note: " << note << '\n';
    }
    return out.str();
}

bool ragDisabled()
{
    const auto value = envOr("TEXTRPG_RAG", "");
    return value == "0" || value == "off" || value == "false";
}

} // namespace

std::string buildRagContext(const GameState& state, const std::string& query)
{
    const auto chunks = buildChunks(state);
    if (chunks.empty() || query.empty() || ragDisabled()) {
        return formatChunks(chunks);
    }

    try {
        ollama::setServerURL(envOr("TEXTRPG_RAG_ENDPOINT", envOr("OLLAMA_ENDPOINT", "http://localhost:11434")));
        ollama::setConnectionTimeout(1);
        ollama::setReadTimeout(10);
        ollama::setWriteTimeout(10);

        std::vector<std::pair<std::string, std::vector<float>>> db;
        const auto model = envOr("TEXTRPG_RAG_MODEL", "nomic-embed-text");
        for (const auto& chunk : chunks) {
            ollama::RAG::RAG_add_chunk_to_database(db, model, chunk);
        }

        std::ostringstream out;
        for (const auto& [text, score] : ollama::RAG::RAG_retrieve(db, model, query, 8)) {
            out << "- " << text << " (similarity=" << score << ")\n";
        }
        return out.str().empty() ? formatChunks(chunks, "embedding RAG returned no match") : out.str();
    } catch (const std::exception& ex) {
        return formatChunks(chunks, std::string("embedding RAG failed: ") + ex.what());
    }
}

} // namespace textrpg::llm::internals
