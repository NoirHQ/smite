add_library(tendermint_p2p STATIC
  lib.cpp
)
target_include_directories(tendermint_p2p PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(tendermint_p2p
  noir::common::v1
)

add_library(tendermint::p2p ALIAS tendermint_p2p)

#add_noir_test(channel_test test/channel_test.cpp DEPENDS tendermint::p2p)
