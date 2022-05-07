add_library(noir_log_v1 STATIC
  setup.cpp
)
target_include_directories(noir_log_v1 PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(noir_log_v1
  noir::thread
  spdlog::spdlog
)

add_library(noir::log::v1 ALIAS noir_log_v1)
