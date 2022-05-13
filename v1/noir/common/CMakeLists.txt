add_library(noir_common_v1 STATIC
  chrono.cpp
  mem_clr.cpp
)
target_include_directories(noir_common_v1 PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(noir_common_v1
  cppcodec::cppcodec
  fmt::fmt
)

add_library(noir::common::v1 ALIAS noir_common_v1)

add_noir_test(bytes_test_v1 test/bytes_test.cpp DEPENDS noir::common::v1)
add_noir_test(chrono_test test/chrono_test.cpp DEPENDS noir::common::v1)
add_noir_test(string_test_v1 test/string_test.cpp DEPENDS noir::common::v1)
add_noir_test(varint_test_v1 test/varint_test.cpp DEPENDS noir::common::v1)
