#pragma once
#include <noir/net/consensus/validator.h>
#include <noir/net/consensus/params.h>
#include <noir/net/protocol.h>

namespace noir::net::consensus { // todo - move consensus somewhere (maybe under libs?)

using namespace std;

/**
 * State is a short description of the latest committed block of the Tendermint consensus.
 * It keeps all information necessary to validate new blocks,
 * including the last validator set and the consensus params.
 */
class state {
 public:
  string version;

  string chain_id;
  int64_t initial_height;

  int64_t last_block_height{0}; // set to 0 at genesis
  block_id last_block_id;
  tstamp last_block_time;

  validator_set next_validators;
  validator_set
      validators; // persisted to the database separately every time they change so we can query for historical validator sets.
  validator_set last_validators; // used to validate block.LastCommit
  int64_t last_height_validators_changed;

  consensus_params consensus_params;
  int64_t last_height_consensus_params_changed;

  bytes last_result_hash;

  bytes app_hash;
};

}
