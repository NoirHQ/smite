#pragma once
#include <noir/net/types.h>
#include <noir/net/consensus/validator.h>
#include <noir/net/consensus/vote_set.h>

namespace noir::net::consensus { // todo - where to place?

struct part {
  uint32_t index;
  bytes bytes;
  //proof proof;
};

struct part_set {
  uint32_t total;
  bytes hash;

//  std::mutex mtx;
  std::vector<part> parts;
  //bit_array parts_bit_array;
  uint32_t count;
  int64_t byte_size;
};

struct round_vote_set {
  vote_set prevotes;
  vote_set precommits;
};

/**
 * Keeps track of all VoteSets from round 0 to round 'round'.
 * Also keeps track of up to one RoundVoteSet greater than
 * 'round' from each peer, to facilitate catchup syncing of commits.
 *
 * A commit is +2/3 precommits for a block at a round,
 * but which round is not known in advance, so when a peer
 * provides a precommit for a round greater than mtx.round,
 * we create a new entry in roundVoteSets but also remember the
 * peer to prevent abuse.
 * We let each peer provide us with up to 2 unexpected "catchup" rounds.
 * One for their LastCommit round, and another for the official commit round.
 */
struct height_vote_set {
  std::string chain_id;
  int64_t height;
  validator_set val_set;

//  std::mutex mtx;
  int32_t round;
  std::map<int32_t, round_vote_set> round_vote_sets;
  std::map<node_id, int32_t> peer_catchup_rounds;
};

}
