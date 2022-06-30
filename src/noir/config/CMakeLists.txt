add_library(noir_config STATIC
  mempool.cpp
)
target_include_directories(noir_config PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(noir_config
  appbase
)

add_library(noir::config ALIAS noir_config)
