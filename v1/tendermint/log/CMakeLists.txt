add_library(tendermint_log STATIC
  log.cpp
  setup.cpp
)
target_include_directories(tendermint_log PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(tendermint_log
  tendermint::core
  noir::log::v1
)

add_library(tendermint::log ALIAS tendermint_log)
