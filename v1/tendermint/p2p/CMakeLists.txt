add_library(tendermint_p2p STATIC
  lib.cpp
  channel.cpp
)
target_include_directories(tendermint_p2p PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(tendermint_p2p
  noir::common::v1
)

add_library(tendermint::p2p ALIAS tendermint_p2p)

add_noir_example(receive_channel_example example/receive_channel_example.cpp DEPENDS tendermint::p2p)
add_noir_example(merge_channel_example example/merge_channel_example.cpp DEPENDS tendermint::p2p)
