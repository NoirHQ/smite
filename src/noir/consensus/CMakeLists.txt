add_library(noir_consensus STATIC
  block.cpp
  block_sync/block_pool.cpp
  block_sync/reactor.cpp
  consensus_reactor.cpp
  consensus_state.cpp
  indexer/event_sink.h
  indexer/sink/null/null.h
  indexer/sink/psql/psql.h
  merkle/proof.cpp
  merkle/tree.cpp
  priv_validator.cpp
  privval/file.cpp
  replay.cpp
  types/genesis.cpp
  validation.cpp
  validator.cpp
  vote.cpp
  wal.cpp
)
target_link_libraries(noir_consensus
  libpqxx::pqxx
  noir_common
  noir_crypto
  noir_tx_pool
  RocksDB::rocksdb
)
set_target_properties(noir_consensus PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::consensus ALIAS noir_consensus)

add_noir_test(bit_array_test test/bit_array_test.cpp DEPENDS noir_consensus)
add_noir_test(block_executor_test test/block_executor_test.cpp DEPENDS noir_consensus)
add_noir_test(block_test test/block_test.cpp DEPENDS noir_consensus)
add_noir_test(consensus_state_test test/consensus_state_test.cpp DEPENDS noir_consensus)
add_noir_test(events_test test/event_bus_test.cpp DEPENDS noir_consensus)
add_noir_test(privval_test privval/test/file_test.cpp DEPENDS noir_consensus)
add_noir_test(psql_test indexer/sink/psql/test/psql_test.cpp DEPENDS noir_consensus)
add_noir_test(replay_test test/replay_test.cpp DEPENDS noir_consensus)
add_noir_test(tree_test merkle/test/tree_test.cpp DEPENDS noir_consensus)
add_noir_test(validator_test test/validator_test.cpp DEPENDS noir_consensus)
add_noir_test(wal_test test/wal_test.cpp DEPENDS noir_consensus)
add_noir_test(store_test store/test/state_store_test.cpp store/test/block_store_test.cpp DEPENDS noir_consensus)
add_noir_test(multiple_vals_test test/multiple_vals_test.cpp)
