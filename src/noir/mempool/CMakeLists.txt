add_library(noir_mempool STATIC
  cache.cpp
  ids.cpp
  tx.cpp
)
add_library(noir::mempool ALIAS noir_mempool)
target_link_libraries(noir_mempool
  noir::config
  noir::consensus
  noir::log
)

add_noir_test(mempool_cache_test test/cache_test.cpp DEPENDS noir::mempool)
add_noir_test(mempool_ids_test test/ids_test.cpp DEPENDS noir::mempool)
add_noir_test(mempool_priority_queue_test test/priority_queue_test.cpp DEPENDS noir::mempool)
add_noir_test(mempool_tx_test test/tx_test.cpp DEPENDS noir::mempool)
add_noir_test(mempool_test test/mempool_test.cpp DEPENDS noir::mempool)

add_noir_benchmark(mempool_cache_bench_test test/cache_bench_test.cpp DEPENDS noir::mempool)
