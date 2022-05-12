add_library(noir_common STATIC
  helper/cli.cpp
  hex.cpp
  log.cpp
  thread_pool.cpp
  time.cpp
  types.cpp
)
target_include_directories(noir_common PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(noir_common
  appbase
  fc
  fmt::fmt
  nonstd::scope-lite
  range-v3::range-v3
  tl::expected
)
set_target_properties(noir_common PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::common ALIAS noir_common)

add_noir_test(bytes_test test/bytes_test.cpp DEPENDS noir_common)
add_noir_test(check_test test/check_test.cpp DEPENDS noir_common)
add_noir_test(hex_test test/hex_test.cpp DEPENDS noir_common)
add_noir_test(time_test test/time_test.cpp DEPENDS noir_common)
add_noir_test(varint_test types/test/varint_test.cpp DEPENDS noir_common)
add_noir_test(helper_test helper/test/variant_test.cpp DEPENDS noir_common)
