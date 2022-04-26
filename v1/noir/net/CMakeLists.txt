add_library(noir_net_v1 INTERFACE)
target_include_directories(noir_net_v1 INTERFACE ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(noir_net_v1 INTERFACE
  noir::log
)

add_library(noir::net::v1 ALIAS noir_net_v1)

add_executable(tcp_pingpong test/tcp_pingpong.cpp)
target_link_libraries(tcp_pingpong noir::net::v1)
