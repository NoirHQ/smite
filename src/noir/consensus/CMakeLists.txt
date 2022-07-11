add_library(noir_consensus STATIC
  block_sync/block_pool.cpp
  block_sync/reactor.cpp
  consensus_reactor.cpp
  consensus_state.cpp
  crypto.cpp
  node.cpp
  ev/evidence_pool.cpp
  ev/reactor.cpp
  indexer/sink/psql/psql.cpp
  merkle/proof.cpp
  merkle/tree.cpp
  privval/file.cpp
  replay.cpp
  types/block.cpp
  types/evidence.cpp
  types/genesis.cpp
  types/node_key.cpp
  types/priv_validator.cpp
  types/proposal.cpp
  types/validation.cpp
  types/validator.cpp
  types/vote.cpp
  wal.cpp
)
target_link_libraries(noir_consensus
  RocksDB::rocksdb # Must come before proto
  libpqxx::pqxx
  noir::clist
  noir::codec
  noir::common
  noir::core
  noir::crypto
  noir::proto
  tendermint::log
  sodium
)
set_target_properties(noir_consensus PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::consensus ALIAS noir_consensus)

add_noir_test(bit_array_test test/bit_array_test.cpp DEPENDS noir_consensus)
add_noir_test(block_executor_test test/block_executor_test.cpp DEPENDS noir_consensus)
add_noir_test(block_test types/test/block_test.cpp DEPENDS noir_consensus)
add_noir_test(consensus_state_test test/consensus_state_test.cpp DEPENDS noir_consensus)
add_noir_test(crypto_ed25519_test test/crypto_ed25519_test.cpp DEPENDS noir_consensus)
add_noir_test(events_test types/test/event_bus_test.cpp DEPENDS noir_consensus)
add_noir_test(evidence_pool_test ev/test/evidence_pool_test.cpp DEPENDS noir_consensus)
add_noir_test(evidence_test types/test/evidence_test.cpp DEPENDS noir_consensus)
add_noir_test(evidence_verify_test ev/test/evidence_verify_test.cpp DEPENDS noir_consensus)
add_noir_test(multiple_vals_test test/multiple_vals_test.cpp)
add_noir_test(node_key_test types/test/node_key_test.cpp DEPENDS noir_consensus)
add_noir_test(privval_test privval/test/file_test.cpp DEPENDS noir_consensus)
add_noir_test(psql_test indexer/sink/psql/test/psql_test.cpp DEPENDS noir_consensus)
add_noir_test(replay_test test/replay_test.cpp DEPENDS noir_consensus)
add_noir_test(store_test store/test/state_store_test.cpp store/test/block_store_test.cpp DEPENDS noir_consensus)
add_noir_test(tree_test merkle/test/tree_test.cpp DEPENDS noir_consensus)
add_noir_test(validator_test types/test/validator_test.cpp DEPENDS noir_consensus)
add_noir_test(vote_test types/test/vote_test.cpp DEPENDS noir_consensus)
add_noir_test(wal_test test/wal_test.cpp DEPENDS noir_consensus)
