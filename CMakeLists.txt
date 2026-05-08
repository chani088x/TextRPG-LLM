cmake_minimum_required(VERSION 3.20)

project(LLM_TextRPG_v2 LANGUAGES CXX)

include(CTest)

find_package(CURL REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_library(llm_module
    src/llm/LLM.cpp
    src/llm/OpenAIChatClient.cpp
    src/llm/OllamaChatClient.cpp
    src/llm/PromptBuilder.cpp
    src/llm/ResponseParser.cpp
    src/llm/StateApplier.cpp
)

add_library(game_module
    src/game/Item.cpp
    src/game/Equipment.cpp
    src/game/Consumable.cpp
    src/game/QuestItem.cpp
    src/game/Inventory.cpp
)

add_library(combat_module
    src/combat/CombatSystem.cpp
)

add_library(npc_module
    src/NPC/NPCManager.cpp
)

target_include_directories(npc_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(npc_module PUBLIC llm_module)

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

target_include_directories(llm_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ollama
)

if(WIN32)
    target_link_libraries(llm_module PUBLIC ws2_32)
endif()

target_link_libraries(llm_module PUBLIC CURL::libcurl)

add_executable(llm_text_rpg
    main.cpp
)

target_link_libraries(llm_text_rpg
    PRIVATE
        game_module
        llm_module
        combat_module
        npc_module
)

if(BUILD_TESTING)
    add_executable(llm_engine_tests
        tests/test_llm_engine.cpp
    )

    target_link_libraries(llm_engine_tests
        PRIVATE
            llm_module
    )

    add_test(NAME llm_engine_tests COMMAND $<TARGET_FILE:llm_engine_tests>)
endif()
