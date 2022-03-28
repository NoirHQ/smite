add_library(noir_tx_pool STATIC
  tx_pool.cpp
  )
target_link_libraries(noir_tx_pool
  noir_common
  noir_crypto
  )
set_target_properties(noir_tx_pool PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::tx_pool ALIAS noir_tx_pool)

add_noir_test(tx_pool_test test/tx_pool_test.cpp test/unapplied_tx_queue_test.cpp test/LRU_cache_test.cpp DEPENDS noir_tx_pool)
