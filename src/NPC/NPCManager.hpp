#pragma once
#include "llm/LLMTypes.hpp"
#include <string>
#include <map>
#include <vector>

namespace textrpg::npc {


    struct NPCProfile
    {
        // NPC의 고유한 특성 (이름, 성격, 말투)
        std::string name;
        std::string personality;
        std::string speechStyle;
        int affinity = 0; // NPC와의 친밀도, -100에서 100 사이
        bool isMet = false;
    };

    class NPCManager
    {
    private:
        std::map<std::string, NPCProfile> npcDatabase; // NPC 이름을 키로 하는 데이터베이스
    
    public:
        NPCManager(){
            addNPC("알렉산더", "쾌활하고 에너지가 넘침", "호탕한 말투"); // 예시 NPC 추가
    
        }

        void addNPC(const std::string& name, const std::string& personality, const std::string& speechStyle){
            NPCProfile profile;
            profile.name = name;
            profile.personality = personality;
            profile.speechStyle = speechStyle;
            npcDatabase[name] = profile;
        }

        void addGeneratedNPC(const textrpg::llm::GeneratedNPC& newNpc){
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
        void updateAffinity(const std::string& name, int amount){
            if (npcDatabase.find(name) != npcDatabase.end()){
                npcDatabase[name].affinity += amount;
            }
        }

        std::string generateNPCPrompt(const std::string& location){
            for(auto& [name, profile] : npcDatabase){
                if(location.find(name) != std::string::npos){
                    profile.isMet = true;

                    std::string prompt = "\n 현재 등장 NPC 정보 ### \n";
                    prompt += "이름: " + profile.name + "\n";
                    prompt += "성격: " + profile.personality + "\n";
                    prompt += "말투: " + profile.speechStyle + "\n";

                    // 친밀도에 따른 태도 설명 추가
                    if(profile.affinity >= 50){
                        prompt += "태도: 당신을 아끼고 매우 협조적입니다.\n";
                    } else if(profile.affinity < 0){
                        prompt += "태도: 당신을 경계하고 무례하게 대합니다.\n";
                    } else {
                        prompt += "태도: 낯선 여행자로 대하며 적당히 친절하지만 경계하고 있습니다.\n";
                    }

                    prompt += "NPC가 당신에게 말을 걸 때, 위의 정보를 바탕으로 NPC의 성격과 말투에 맞게 대화 내용을 생성하세요.";
                    return prompt;
                }
            }
            return "";
        }
    };

} // namespace textrpg::npc