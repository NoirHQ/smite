add_library(noir_config STATIC
  defaults.cpp
)
target_include_directories(noir_config PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(noir_config
  noir::common
)

add_library(noir::config ALIAS noir_config)
