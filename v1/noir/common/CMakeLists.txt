add_library(noir_common_v1 STATIC
  chrono.cpp
  hex.cpp
)
target_include_directories(noir_common_v1 PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(noir_common_v1
  fmt::fmt
)

add_library(noir::common::v1 ALIAS noir_common_v1)

add_noir_test(chrono_test test/chrono_test.cpp DEPENDS noir::common::v1)
add_noir_test(string_test_v1 test/string_test.cpp DEPENDS noir::common::v1)
add_noir_test(varint_test_v1 test/varint_test.cpp DEPENDS noir::common::v1)
add_noir_test(buffered_writer_test test/buffered_writer_test.cpp DEPENDS noir::common::v1)

add_noir_example(ticker_example example/ticker_example.cpp DEPENDS noir::common::v1)
