add_library(tendermint_net STATIC
  conn.cpp
)
target_include_directories(tendermint_net PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(tendermint_net
  noir::log
  tendermint::common
)

add_library(tendermint::net ALIAS tendermint_net)
