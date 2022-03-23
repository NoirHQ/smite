add_library(noir_jmt STATIC
  constants.cpp
  types.cpp
)
target_link_libraries(noir_jmt noir_common noir_crypto)
set_target_properties(noir_jmt PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::jmt ALIAS noir_jmt)

add_noir_test(jmt_test test/jmt_test.cpp DEPENDS noir_jmt)
add_noir_test(jmt_node_types_test types/test/node_test.cpp DEPENDS noir_jmt)
add_noir_test(jmt_tree_cache_test types/test/tree_cache_test.cpp DEPENDS noir_jmt)
