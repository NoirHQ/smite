add_library(noir_jmt STATIC
  constants.cpp
  types.cpp
)
target_link_libraries(noir_jmt
  noir::codec
  noir::common
  noir::crypto
)

add_library(noir::jmt ALIAS noir_jmt)

add_noir_test(jmt_test test/jmt_test.cpp DEPENDS noir::jmt)
add_noir_test(jmt_node_types_test types/test/node_test.cpp DEPENDS noir::jmt)
add_noir_test(jmt_tree_cache_test types/test/tree_cache_test.cpp DEPENDS noir::jmt)
