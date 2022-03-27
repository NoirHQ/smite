add_library(tendermint_log STATIC
  config.cpp
)
target_include_directories(tendermint_log PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(tendermint_log
  spdlog::spdlog
)

add_library(tendermint::log ALIAS tendermint_log)
