target_sources(noir
  PRIVATE
    cli_helper.cpp
    hex.cpp
    thread_pool.cpp
    log.cpp
    types.cpp
)

add_noir_test(check_test test/check_test.cpp)
add_noir_test(common_types_test types_test.cpp)
