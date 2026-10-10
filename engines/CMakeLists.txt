qt_add_library(mnaka_engines STATIC assistant/ConversationEngine.h assistant/ConversationEngine.cpp
    skills/SkillValidator.h skills/SkillValidator.cpp)
target_include_directories(mnaka_engines PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(mnaka_engines PUBLIC mnaka_core mnaka_adapters)
