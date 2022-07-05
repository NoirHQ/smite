add_library(tendermint_abci STATIC
  client/client.cpp
  types/messages.cpp
)
target_include_directories(tendermint_abci PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tendermint_abci
  noir::codec
  noir::common
  noir::log
  noir::net
  noir::proto
  #tendermint::common
  tendermint::service
)

add_library(tendermint::abci ALIAS tendermint_abci)

add_executable(socket_client_test client/test/socket_client_test.cpp)
target_link_libraries(socket_client_test tendermint::abci)
