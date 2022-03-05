target_sources(noir
  PRIVATE
    constants.cpp
    types.cpp
)

add_noir_test(jmt_test test/jmt_test.cpp)
add_noir_test(jmt_node_types_test types/test/node_test.cpp)
add_noir_test(jmt_tree_cache_test types/test/tree_cache_test.cpp)
