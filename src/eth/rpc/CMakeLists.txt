target_sources(noir
  PRIVATE
    rpc.cpp
)

add_noir_test(eth_rpc_test rpc_test.cpp)