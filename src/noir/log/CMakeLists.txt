add_library(noir_log STATIC
  config.cpp
)
target_include_directories(noir_log PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(noir_log
  spdlog::spdlog
)

add_library(noir::log ALIAS noir_log)
