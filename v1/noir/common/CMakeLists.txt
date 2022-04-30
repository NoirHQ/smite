add_library(noir_common_v1 STATIC
  hex.cpp
)
target_include_directories(noir_common_v1 PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(noir_common_v1
  fmt::fmt
)

add_library(noir::common::v1 ALIAS noir_common_v1)

add_noir_test(varint_test_v1 test/varint_test.cpp DEPENDS noir::common::v1)
