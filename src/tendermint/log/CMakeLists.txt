add_library(tendermint_log STATIC
  log.cpp
  setup.cpp
)
target_include_directories(tendermint_log PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(tendermint_log
  noir::log
)

add_library(tendermint::log ALIAS tendermint_log)
