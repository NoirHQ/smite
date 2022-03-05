target_sources(noir
  PRIVATE
  tx_pool.cpp
  )

add_noir_test(tx_pool_test test/tx_pool_test.cpp)
