#pragma once

#include "llm/LLMTypes.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace textrpg::npc {

struct NPCProfile {
    std::string name;
    std::vector<std::string> aliases;
    std::string personality;
    std::string speechStyle;
    std::string homeLocation;
    int affinity = 0;
    bool isMet = false;
    std::optional<textrpg::llm::Quest> availableQuest;
};

class NPCManager {
public:
    NPCManager();

    void addNPC(
        const std::string& name,
        const std::vector<std::string>& aliases,
        const std::string& personality,
        const std::string& speechStyle,
        const std::string& home = "",
        std::optional<textrpg::llm::Quest> quest = std::nullopt);

    void addNPC(
        const std::string& name,
        const std::string& personality,
        const std::string& speechStyle);

    void addGeneratedNPC(const textrpg::llm::GeneratedNPC& newNpc);
    void updateAffinity(const std::string& name, int amount);
    std::string generateNPCPrompt(const std::string& input);

private:
    std::map<std::string, NPCProfile> npcDatabase_;
};

} // namespace textrpg::npc
