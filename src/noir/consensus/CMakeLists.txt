target_sources(noir
  PRIVATE
  block.cpp
  block_sync/block_pool.cpp
  block_sync/reactor.cpp
  consensus_reactor.cpp
  consensus_state.cpp
  merkle/proof.cpp
  merkle/tree.cpp
  priv_validator.cpp
  replay.cpp
  validator.cpp
  vote.cpp
  wal.cpp)

add_noir_test(bit_array_test test/bit_array_test.cpp)
add_noir_test(block_executor_test test/block_executor_test.cpp)
add_noir_test(block_test test/block_test.cpp)
add_noir_test(consensus_state_test test/consensus_state_test.cpp)
add_noir_test(replay_test test/replay_test.cpp)
add_noir_test(tree_test merkle/test/tree_test.cpp)
add_noir_test(validator_test test/validator_test.cpp)
add_noir_test(wal_test test/wal_test.cpp)
add_noir_test(store_test store/test/state_store_test.cpp store/test/block_store_test.cpp)
