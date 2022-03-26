add_library(noir_rpc STATIC
  jsonrpc.cpp
  jsonrpc/endpoint.cpp
  rpc.cpp
  websocket/websocket.cpp
)
target_link_libraries(noir_rpc
  noir_common
  noir_crypto
  websocketpp
)
set_target_properties(noir_rpc PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::rpc ALIAS noir_rpc)

add_noir_test(resource_test test/resource_test.cpp)
