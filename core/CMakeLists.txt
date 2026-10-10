qt_add_library(mnaka_core STATIC
    contracts/ToolIntent.h contracts/ToolIntent.cpp
    contracts/SchemaValidator.h contracts/SchemaValidator.cpp
    policy/ToolBroker.h policy/ToolBroker.cpp
    orchestrator/EngineService.h orchestrator/EngineRegistry.h orchestrator/EngineRegistry.cpp
    orchestrator/TaskStates.h orchestrator/TaskStore.h orchestrator/TaskStore.cpp
    orchestrator/TaskOrchestrator.h orchestrator/TaskOrchestrator.cpp)
target_include_directories(mnaka_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(mnaka_core PUBLIC Qt6::Core)
qt_add_resources(mnaka_core contracts PREFIX /mnaka/schemas BASE ../schemas
    FILES ../schemas/intent.schema.json ../schemas/plan.schema.json
          ../schemas/task-request.schema.json ../schemas/event.schema.json
          ../schemas/task-result.schema.json ../schemas/approval.schema.json
          ../schemas/skill.schema.json ../schemas/rule.schema.json ../schemas/sync.schema.json)
