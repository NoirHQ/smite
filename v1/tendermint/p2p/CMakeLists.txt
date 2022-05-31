add_library(tendermint_p2p STATIC
  lib.cpp
  channel.cpp
  conn/connection.cpp
  conn/merlin.cpp
  conn/secret_connection.cpp
  transport_mconn.cpp
)
target_include_directories(tendermint_p2p PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(tendermint_p2p
  noir::core::v1
  noir::common::v1
  noir::net::v1
  tendermint::proto
  OpenSSL
)

add_library(tendermint::p2p ALIAS tendermint_p2p)

add_noir_example(receive_channel_example example/receive_channel_example.cpp DEPENDS tendermint::p2p)
add_noir_example(merge_channel_example example/merge_channel_example.cpp DEPENDS tendermint::p2p)

add_noir_test(channel_test test/channel_test.cpp DEPENDS tendermint::p2p)
add_noir_test(connection_test conn/test/connection_test.cpp DEPENDS tendermint::p2p)
