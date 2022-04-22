include(NoirTest)

set(NOIR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(noir STATIC)
target_link_libraries(noir
  noir::common
  noir::consensus
  noir::crypto
  noir::jmt
  noir::p2p
  noir::rpc
  noir::tx_pool
  tendermint::proto
  softfloat
)

add_subdirectory(noir)
add_subdirectory(tendermint)
