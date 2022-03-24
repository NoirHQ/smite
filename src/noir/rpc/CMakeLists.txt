target_sources(noir
  PRIVATE
    jsonrpc/endpoint.cpp
    jsonrpc.cpp
    rpc.cpp
)

add_noir_test(resource_test test/resource_test.cpp)
