add_library(tendermint_p2p STATIC
  conn/connection.cpp
  conn/merlin.cpp
  conn/secret_connection.cpp
  channel.cpp
  lib.cpp
  peermanager.cpp
  transport_mconn.cpp
  router.cpp
)
target_include_directories(tendermint_p2p PUBLIC ${PROJECT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR})
target_link_libraries(tendermint_p2p
  eo::eo
  noir::common
  noir::net
  noir::proto
)

add_library(tendermint::p2p ALIAS tendermint_p2p)

add_noir_test(tendermint_secret_connection_test conn/test/secret_connection_test.cpp DEPENDS tendermint::p2p)
add_noir_test(waker_test test/waker_test.cpp DEPENDS tendermint::p2p)
