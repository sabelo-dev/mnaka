qt_add_library(mnaka_adapters STATIC models/ModelProvider.h mock/MockModelProvider.h mock/MockModelProvider.cpp
    supabase/SupabaseAuth.h supabase/SupabaseAuth.cpp)
target_include_directories(mnaka_adapters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(mnaka_adapters PUBLIC mnaka_core PRIVATE Qt6::Network)
