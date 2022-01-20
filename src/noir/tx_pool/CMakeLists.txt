target_sources(noir
  PRIVATE
  tx_pool.cpp
)

if(BUILD_TESTING)
  add_noir_test(tx_pool_test tx_pool_test.cpp)
endif()