add_library(noir_thread STATIC thread_name.cpp)
target_include_directories(noir_thread PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(noir_thread spdlog::spdlog)

add_library(noir::thread ALIAS noir_thread)
