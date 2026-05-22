#pragma once
#include "llm/LLMTypes.hpp"
#include <string>
#include <map>
#include <vector>
#include <optional>

namespace textrpg::npc {


    struct NPCProfile
    {
        // NPC의 고유한 특성 (이름, 별명, 성격, 말투, 지역...)
        std::string name;
        std::vector<std::string> aliases; // NPC의 별명이나 다른 이름들
        std::string personality;
        std::string speechStyle;
        std::string homeLocation;
        int affinity = 0; // NPC와의 친밀도, -100에서 100 사이
        bool isMet = false;

        std::optional<textrpg::llm::Quest> availableQuest;
    };

    class NPCManager
    {
    private:
        std::map<std::string, NPCProfile> npcDatabase; // NPC 이름을 키로 하는 데이터베이스
    
    public:
        NPCManager();

        void addNPC(const std::string& name,
                    const std::vector<std::string>& aliases,
                    const std::string& personality,
                    const std::string& speechStyle,
                    const std::string& home = "",
                    std::optional<textrpg::llm::Quest> quest = std::nullopt);
        void addGeneratedNPC(const textrpg::llm::GeneratedNPC& newNpc);

        void updateAffinity(const std::string& name, int amount);

        std::string generateNPCPrompt(const std::string& input);
        
    };


} // namespace textrpg::npc
