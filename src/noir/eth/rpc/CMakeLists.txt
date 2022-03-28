target_sources(noir
  PRIVATE
    api.cpp
    rpc.cpp
)

add_noir_test(eth_rpc_test test/api_test.cpp)
