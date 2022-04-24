add_library(noir_common_v1 INTERFACE)
target_include_directories(noir_common_v1 INTERFACE ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(noir_common_v1 INTERFACE
  fmt::fmt
)

add_library(noir::common::v1 ALIAS noir_common_v1)

add_noir_test(result_test test/result_test.cpp DEPENDS noir::common::v1)
