target_sources(noir
  PRIVATE
    helper/cli.cpp
    hex.cpp
    thread_pool.cpp
    log.cpp
    types.cpp
)

add_noir_test(bytes_test test/bytes_test.cpp)
add_noir_test(check_test test/check_test.cpp)
add_noir_test(hex_test test/hex_test.cpp)
add_noir_test(varint_test types/test/varint_test.cpp)
add_noir_test(helper_test helper/test/variant_test.cpp)
