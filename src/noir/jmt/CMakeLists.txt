target_sources(noir
  PRIVATE
    constants.cpp
    types.cpp
)

add_noir_test(jmt_test ${CMAKE_CURRENT_SOURCE_DIR}/jmt_test.cpp)
add_noir_test(jmt_node_types_test ${CMAKE_CURRENT_SOURCE_DIR}/types/node_test.cpp)
add_noir_test(jmt_tree_cache_test ${CMAKE_CURRENT_SOURCE_DIR}/types/tree_cache_test.cpp)
