add_library(noir_log_v1 INTERFACE)
target_include_directories(noir_log_v1 INTERFACE ${NOIR_SOURCE_DIR})
target_link_libraries(noir_log_v1 INTERFACE
  spdlog::spdlog
)

add_library(noir::log::v1 ALIAS noir_log_v1)
