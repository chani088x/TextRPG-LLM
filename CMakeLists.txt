cmake_minimum_required(VERSION 3.20)

project(LLM_TextRPG_v2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_library(game_module
    src/game/Item.cpp
    src/game/Equipment.cpp
    src/game/Consumable.cpp
    src/game/QuestItem.cpp
    src/game/Inventory.cpp
)

target_include_directories(game_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(game_module PUBLIC llm_module)

add_library(llm_module
    src/llm/ContextBuilder.cpp
    src/llm/LLMConfig.cpp
    src/llm/LLMEventValidator.cpp
    src/llm/LLMFallbackFactory.cpp
    src/llm/LLMLogger.cpp
    src/llm/LLMOutputParser.cpp
    src/llm/LLMService.cpp
    src/llm/OllamaClient.cpp
    src/llm/PromptBuilder.cpp
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

enable_testing()

add_executable(llm_module_tests
    tests/test_llm_module.cpp
)

target_link_libraries(llm_module_tests
    PRIVATE
        llm_module
)

add_test(NAME llm_module_tests COMMAND llm_module_tests)

add_executable(llm_text_rpg
    main.cpp
)

target_link_libraries(llm_text_rpg
    PRIVATE
        game_module
        llm_module
)
