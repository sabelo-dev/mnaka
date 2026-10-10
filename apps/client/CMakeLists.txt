# WorkspaceModel is compiled once here; the client and workspace_tests both link it.
qt_add_library(mnaka_client_module STATIC)
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
# The module sits under the build root so editor tooling resolves the import.
qt_add_qml_module(mnaka_client_module URI Mnaka.App VERSION 1.0 OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mnaka/App QML_FILES qml/Main.qml SOURCES WorkspaceModel.h WorkspaceModel.cpp)
target_include_directories(mnaka_client_module PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(mnaka_client_module PUBLIC Qt6::Quick Qt6::Concurrent mnaka_core mnaka_engines)
if(EMSCRIPTEN)
    # The browser file chooser is only reachable through Qt's private wasm file access.
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
    target_link_libraries(mnaka_client_module PRIVATE Qt6::GuiPrivate)
endif()
qt_add_executable(mnaka_client main.cpp)
target_link_libraries(mnaka_client PRIVATE Qt6::QuickControls2 mnaka_client_module mnaka_client_moduleplugin)
# Kept beside the tests and the deployed Qt runtime in the build root.
set_target_properties(mnaka_client PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(EMSCRIPTEN)
    # No eval/new Function in the generated glue, so the page runs under a CSP without 'unsafe-eval'.
    target_link_options(mnaka_client PRIVATE -sDYNAMIC_EXECUTION=0)
    # Lets the page pass the account service configuration as environment variables.
    set_target_properties(mnaka_client PROPERTIES QT_WASM_EXTRA_EXPORTED_METHODS ENV)
endif()
install(TARGETS mnaka_client BUNDLE DESTINATION . RUNTIME DESTINATION bin)
