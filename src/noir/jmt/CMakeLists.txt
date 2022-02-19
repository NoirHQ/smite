target_sources(noir
  PRIVATE
    constants.cpp
)

add_noir_test(jmt_node_types_test ${CMAKE_CURRENT_SOURCE_DIR}/types/node_test.cpp)
