# WorkspaceModel is compiled once here; the client and workspace_tests both link it.
qt_add_library(mnaka_client_module STATIC)
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
# The module sits under the build root so editor tooling resolves the import.
qt_add_qml_module(mnaka_client_module URI Mnaka.App VERSION 1.0 OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mnaka/App QML_FILES qml/Main.qml RESOURCES assets/logo.png SOURCES WorkspaceModel.h WorkspaceModel.cpp DeveloperSession.h DeveloperSession.cpp VoiceInput.h VoiceInput.cpp)
# Main.qml is large: compiling every binding to C++ overflows MinGW's object format and takes
# many minutes for WebAssembly. Bytecode alone loads just as early and compiles in seconds.
set_target_properties(mnaka_client_module PROPERTIES QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode")
target_include_directories(mnaka_client_module PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(mnaka_client_module PUBLIC Qt6::Quick Qt6::Concurrent mnaka_core mnaka_engines)
if(EMSCRIPTEN)
    # The browser file chooser is only reachable through Qt's private wasm file access.
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
    target_link_libraries(mnaka_client_module PRIVATE Qt6::GuiPrivate)
endif()
# Account service the client talks to when the environment does not say otherwise. Android has
# no environment to configure, so its package is built with these. Both values are public by design.
set(MNAKA_SUPABASE_URL "" CACHE STRING "Default Supabase project URL compiled into the client")
set(MNAKA_SUPABASE_PUBLISHABLE_KEY "" CACHE STRING "Default Supabase publishable key compiled into the client")
if(MNAKA_SUPABASE_PUBLISHABLE_KEY AND NOT MNAKA_SUPABASE_PUBLISHABLE_KEY MATCHES "^sb_publishable_")
    message(FATAL_ERROR "MNAKA_SUPABASE_PUBLISHABLE_KEY must be a Supabase publishable key; never compile in a secret key")
endif()
target_compile_definitions(mnaka_client_module PRIVATE
    MNAKA_DEFAULT_SUPABASE_URL="${MNAKA_SUPABASE_URL}" MNAKA_DEFAULT_SUPABASE_KEY="${MNAKA_SUPABASE_PUBLISHABLE_KEY}")
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
if(ANDROID)
    set_target_properties(mnaka_client PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.mnaka.client"
        QT_ANDROID_APP_NAME "MNAKA"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    # Qt has no TLS of its own on Android: HTTPS needs OpenSSL shipped inside the package.
    # Pinned to one commit of KDAB's prebuilt libraries, the source Qt's documentation names.
    include(FetchContent)
    FetchContent_Declare(android_openssl
        URL https://github.com/KDAB/android_openssl/archive/b71f1470962019bd89534a2919f5925f93bc5779.zip
        URL_HASH SHA256=9277d62ecdb4809801e2c369e0a639c154e0d9137e8d60863b44bf07d16ed5b3
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
    add_android_openssl_libraries(mnaka_client)
endif()
install(TARGETS mnaka_client BUNDLE DESTINATION . RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
