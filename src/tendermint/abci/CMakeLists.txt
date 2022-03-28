add_library(tendermint_abci STATIC
  client/client.cpp
  client/socket_client.cpp
  types/messages.cpp
)
target_include_directories(tendermint_abci PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(tendermint_abci
  tendermint::common
  tendermint::log
  tendermint::proto
)

add_library(tendermint::abci ALIAS tendermint_abci)
