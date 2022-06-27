add_library(noir_log INTERFACE)
target_include_directories(noir_log INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(noir_log INTERFACE
  spdlog::spdlog
)

add_library(noir::log ALIAS noir_log)
