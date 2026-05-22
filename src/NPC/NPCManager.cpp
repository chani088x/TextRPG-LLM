#include "NPC/NPCManager.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace textrpg::npc {
namespace {

bool containsAnyAlias(const std::string& input, const NPCProfile& profile)
{
    if (!profile.name.empty() && input.find(profile.name) != std::string::npos) {
        return true;
    }

    return std::any_of(profile.aliases.begin(), profile.aliases.end(), [&input](const std::string& alias) {
        return !alias.empty() && input.find(alias) != std::string::npos;
    });
}

std::string affinityAttitude(int affinity)
{
    if (affinity >= 50) {
        return "당신을 아끼고 매우 협조적입니다.";
    }
    if (affinity < 0) {
        return "당신을 경계하고 무례하게 대합니다.";
    }
    return "낯선 여행자로 대하며 적당히 친절하지만 경계하고 있습니다.";
}

std::vector<std::string> defaultAliases(const std::string& name)
{
    return name.empty() ? std::vector<std::string> {} : std::vector<std::string> {name};
}

} // namespace

NPCManager::NPCManager()
{
    addNPC("알렉산더", {"알렉산더", "Alexander"}, "쾌활하고 에너지가 넘침", "호탕한 말투", "village pub");
}

void NPCManager::addNPC(
    const std::string& name,
    const std::vector<std::string>& aliases,
    const std::string& personality,
    const std::string& speechStyle,
    const std::string& home,
    std::optional<textrpg::llm::Quest> quest)
{
    NPCProfile profile;
    profile.name = name;
    profile.aliases = aliases.empty() ? defaultAliases(name) : aliases;
    profile.personality = personality;
    profile.speechStyle = speechStyle;
    profile.homeLocation = home;
    profile.availableQuest = std::move(quest);
    npcDatabase_[name] = std::move(profile);
}

void NPCManager::addNPC(
    const std::string& name,
    const std::string& personality,
    const std::string& speechStyle)
{
    addNPC(name, defaultAliases(name), personality, speechStyle);
}

void NPCManager::addGeneratedNPC(const textrpg::llm::GeneratedNPC& newNpc)
{
    if (newNpc.name.empty() || npcDatabase_.find(newNpc.name) != npcDatabase_.end()) {
        return;
    }

    NPCProfile profile;
    profile.name = newNpc.name;
    profile.aliases = defaultAliases(newNpc.name);
    profile.personality = newNpc.personality;
    profile.speechStyle = newNpc.speechStyle;
    profile.affinity = newNpc.affinity;
    profile.isMet = newNpc.isMet;
    npcDatabase_[newNpc.name] = std::move(profile);
}

void NPCManager::updateAffinity(const std::string& name, int amount)
{
    const auto it = npcDatabase_.find(name);
    if (it == npcDatabase_.end()) {
        return;
    }

    it->second.affinity = textrpg::llm::clampInt(it->second.affinity + amount, -100, 100);
}

std::string NPCManager::generateNPCPrompt(const std::string& input)
{
    for (auto& [name, profile] : npcDatabase_) {
        if (!containsAnyAlias(input, profile)) {
            continue;
        }

        profile.isMet = true;

        std::ostringstream prompt;
        prompt << "\n[대화규칙]\n";
        prompt << "1. 현재 대화중인 NPC는 " << profile.name << "입니다.\n";
        prompt << "2. 다음 이벤트가 dialogue라면 반드시 '" << profile.name << ": 대사' 형식으로 씁니다.\n";
        prompt << "3. 장면을 마음대로 전환하거나 다른 NPC를 등장시키지 말고, 플레이어와 이 NPC 사이의 대화에 집중합니다.\n";

        if (profile.availableQuest.has_value()) {
            prompt << "\n[보유 퀘스트 정보]\n";
            prompt << "제목: " << profile.availableQuest->title << "\n";
            prompt << "설명: " << profile.availableQuest->description << "\n";
            prompt << "보상: " << profile.availableQuest->reward << "\n";
            prompt << "플레이어가 질문하면 위 퀘스트를 제안하되, 제목과 설명을 반드시 언급합니다.\n";
        }

        prompt << "\n[NPC 정보]\n";
        prompt << "이름: " << profile.name << "\n";
        prompt << "성격: " << profile.personality << "\n";
        prompt << "말투: " << profile.speechStyle << "\n";
        if (!profile.homeLocation.empty()) {
            prompt << "거주지: " << profile.homeLocation << "\n";
        }
        prompt << "태도: " << affinityAttitude(profile.affinity) << "\n";
        return prompt.str();
    }

    return "";
}

} // namespace textrpg::npc
