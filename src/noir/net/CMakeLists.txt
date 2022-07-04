add_library(noir_net INTERFACE)
target_include_directories(noir_net INTERFACE ${PROJECT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR})
target_link_libraries(noir_net INTERFACE
  noir::log
  eo::eo
)

add_library(noir::net ALIAS noir_net)

add_noir_benchmark(tcp_pingpong test/tcp_pingpong.cpp DEPENDS noir::net)
