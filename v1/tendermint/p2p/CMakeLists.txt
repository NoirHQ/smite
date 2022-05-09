add_library(tendermint_p2p STATIC
  lib.cpp
  channel.cpp
  conn/connection.cpp
)
target_include_directories(tendermint_p2p PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(tendermint_p2p
  noir::core::v1
  noir::common::v1
  tendermint::proto
)

add_library(tendermint::p2p ALIAS tendermint_p2p)

add_noir_example(receive_channel_example example/receive_channel_example.cpp DEPENDS tendermint::p2p)
add_noir_example(merge_channel_example example/merge_channel_example.cpp DEPENDS tendermint::p2p)

add_noir_test(channel_test test/channel_test.cpp DEPENDS tendermint::p2p)
