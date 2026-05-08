#include "NPCManager.hpp"

namespace textrpg::npc {
    NPCManager::NPCManager(){
        addNPC("알렉산더", {"알렉산더", "Alexander"}, "쾌활하고 에너지가 넘침", "호탕한 말투", "village pub");
    }

    void NPCManager::addNPC(const std::string& name, const std::vector<std::string>& aliases, 
                            const std::string& personality, const std::string& speechStyle, 
                            const std::string& home, std::optional<textrpg::llm::Quest> quest){
        NPCProfile profile;
        profile.name = name;
        profile.aliases = aliases;
        profile.personality = personality;
        profile.speechStyle = speechStyle;
        profile.homeLocation = home;
        profile.availableQuest = quest;

        npcDatabase[name] = profile;
    }


    void NPCManager::addGeneratedNPC(const textrpg::llm::GeneratedNPC& newNpc){
        if (npcDatabase.find(newNpc.name) == npcDatabase.end()){
            NPCProfile profile;
            profile.name = newNpc.name;
            profile.personality = newNpc.personality;
            profile.speechStyle = newNpc.speechStyle;
            profile.affinity = newNpc.affinity;
            profile.isMet = newNpc.isMet;

            npcDatabase[newNpc.name] = profile;

        }
    }

    void NPCManager::updateAffinity(const std::string& name, int amount){
        if (npcDatabase.find(name) != npcDatabase.end()){
            npcDatabase[name].affinity += amount;
        }
    }

    
    std::string NPCManager::generateNPCPrompt(const std::string& input){
        for(auto& [name, profile] : npcDatabase){
            for(const auto& alias : profile.aliases){
                if (input.find (alias) != std::string::npos){
                    profile.isMet = true;
                }

                std::string prompt = "\n [대화규칙]### \n";
                prompt += "1. 현재 대화중인 NPC는 " + profile.name + "입니다.\n";
                prompt += "2. 다음 이벤트가 'dialogue'라면, 반드시 [" + profile.name + "]의 직접적인 대사 (\"...\")로 응답하세요.\n";
                prompt += "3. 장면을 마음대로 전환하거나 다른 NPC를 등장시키지 마세요. 플레이어와 이 NPC 사이의 대화에만 집중하세요.\n";
                if (profile.availableQuest.has_value()) {
                    prompt += "\n[보유 퀘스트 정보]###\n";
                    prompt += "제목: " + profile.availableQuest->title + "\n";
                    prompt += "설명: " + profile.availableQuest->description + "\n";
                    prompt += "보상: " + profile.availableQuest->reward + "\n";
                    prompt += "지시: 플레이어가 질문하면 위 퀘스트를 제안하십시오. 제안 시에는 반드시 퀘스트 제목 및 설명을 언급해야 합니다. \n";
                }
                    
                prompt += "이름: " + profile.name + "\n";
                prompt += "성격: " + profile.personality + "\n";
                prompt += "말투: " + profile.speechStyle + "\n";
                prompt += "거주지: " + profile.homeLocation + "\n";


                // 친밀도에 따른 태도 설명 추가
                if(profile.affinity >= 50){
                    prompt += "태도: 당신을 아끼고 매우 협조적입니다.\n";
                } else if(profile.affinity < 0){
                    prompt += "태도: 당신을 경계하고 무례하게 대합니다.\n";
                } else {
                    prompt += "태도: 낯선 여행자로 대하며 적당히 친절하지만 경계하고 있습니다.\n";
                }

                
            

                return prompt;
            }
        }
        return "";
    }
} // namespace textrpg::npc
