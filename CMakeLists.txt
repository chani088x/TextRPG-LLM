cmake_minimum_required(VERSION 3.20)

project(LLM_TextRPG_v2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(ftxui CONFIG REQUIRED)

add_library(llm_module
    src/llm/LLM.cpp
    src/llm/OllamaChatClient.cpp
    src/llm/PromptBuilder.cpp
    src/llm/RagContext.cpp
    src/llm/RecordStore.cpp
    src/llm/ResponseParser.cpp
    src/llm/StateApplier.cpp
)

add_library(game_module
    src/game/DangerSystem.cpp
    src/game/Item.cpp
    src/game/Equipment.cpp
    src/game/Consumable.cpp
    src/game/QuestItem.cpp
    src/game/Inventory.cpp
)

add_library(combat_module
    src/combat/CombatSystem.cpp
    src/combat/CombatResolver.cpp
)

add_library(npc_module
    src/NPC/NPCManager.cpp
)

target_include_directories(game_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(game_module PUBLIC llm_module)

target_include_directories(combat_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(combat_module PUBLIC game_module)

target_include_directories(npc_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(npc_module PUBLIC llm_module)

target_include_directories(llm_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ollama
)

if(WIN32)
    target_link_libraries(llm_module PUBLIC ws2_32)
endif()

add_executable(llm_text_rpg
    main.cpp
    src/app/CliView.cpp
    src/app/TextRpgApp.cpp
)

target_link_libraries(llm_text_rpg
    PRIVATE
        game_module
        llm_module
        combat_module
        npc_module
        ftxui::screen
        ftxui::dom
        ftxui::component
)

# Windows에서 PlaySound(winmm) 사용
if(WIN32)
    target_link_libraries(llm_text_rpg PRIVATE winmm)
endif()

add_custom_command(TARGET llm_text_rpg POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:llm_text_rpg>/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:llm_text_rpg>/data/sound"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/data/game_state.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/game_records.example.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/config.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/start_art_4x3.txt"
        "$<TARGET_FILE_DIR:llm_text_rpg>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data/sound"
        "$<TARGET_FILE_DIR:llm_text_rpg>/data/sound"
)
