add_library(eth STATIC)
target_include_directories(eth PUBLIC ${ETH_SOURCE_DIR})
target_link_libraries(eth
  appbase
  noir
)

add_subdirectory(rpc)
