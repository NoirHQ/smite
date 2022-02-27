target_sources(noir
  PRIVATE
    cli_helper.cpp
    hex.cpp
    thread_pool.cpp
    log.cpp
    types.cpp
)

add_noir_test(common_test
  test/bytes_test.cpp
  test/check_test.cpp
)
