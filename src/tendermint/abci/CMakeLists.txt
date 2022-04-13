add_library(tendermint_abci STATIC
  client/client.cpp
  types/messages.cpp
)
target_include_directories(tendermint_abci PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(tendermint_abci
  noir::log
  tendermint::common
  tendermint::net
  tendermint::proto
)

add_library(tendermint::abci ALIAS tendermint_abci)

add_executable(socket_client_test client/test/socket_client_test.cpp)
target_link_libraries(socket_client_test tendermint::abci)
