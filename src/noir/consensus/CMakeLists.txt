target_sources(noir
  PRIVATE
    consensus_state.cpp)

add_noir_test(validator_test validator_test.cpp)
add_noir_test(consensus_state_test consensus_state_test.cpp)
add_noir_test(store_test store/state_store_test.cpp store/block_store_test.cpp)
