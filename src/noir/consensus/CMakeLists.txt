target_sources(noir
  PRIVATE
  block.cpp
  consensus_state.cpp
  merkle/tree.cpp
  priv_validator.cpp
  validator.cpp
  vote.cpp
  wal.cpp)

add_noir_test(bit_array_test bit_array_test.cpp)
add_noir_test(block_test block_test.cpp)
add_noir_test(block_executor_test block_executor_test.cpp)
add_noir_test(consensus_state_test consensus_state_test.cpp)
add_noir_test(validator_test validator_test.cpp)
add_noir_test(wal_test wal_test.cpp)
add_noir_test(store_test store/state_store_test.cpp store/block_store_test.cpp)
add_noir_test(tree_test merkle/tree_test.cpp)
