// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma
#include <noir/consensus/validator.h>
#include <noir/p2p/protocol.h>

namespace noir::consensus {

using P2PID = std::string;

struct block_votes {
  bool peer_maj23;
  // bitarray
  p2p::vote_message votes;
  int64_t sum;
};

/**
 * 	VoteSet helps collect signatures from validators at each height+round for a
        predefined vote type.

        We need VoteSet to be able to keep track of conflicting votes when validators
        double-sign.  Yet, we can't keep track of *all* the votes seen, as that could
        be a DoS attack vector.

        There are two storage areas for votes.
        1. voteSet.votes
        2. voteSet.votesByBlock

        `.votes` is the "canonical" list of votes.  It always has at least one vote,
        if a vote from a validator had been seen at all.  Usually it keeps track of
        the first vote seen, but when a 2/3 majority is found, votes for that get
        priority and are copied over from `.votesByBlock`.

        `.votesByBlock` keeps track of a list of votes for a particular block.  There
        are two ways a &blockVotes{} gets created in `.votesByBlock`.
        1. the first vote seen by a validator was for the particular block.
        2. a peer claims to have seen 2/3 majority for the particular block.

        Since the first vote from a validator will always get added in `.votesByBlock`
        , all votes in `.votes` will have a corresponding entry in `.votesByBlock`.

        When a &blockVotes{} in `.votesByBlock` reaches a 2/3 majority quorum, its
        votes are copied into `.votes`.

        All this is memory bounded because conflicting votes only get added if a peer
        told us to track that block, each peer only gets to tell us 1 such block, and,
        there's only a limited number of peers.

        NOTE: Assumes that the sum total of voting power does not exceed MaxUInt64.
 */
struct vote_set {
  std::string chain_id;
  int64_t height;
  int32_t round;
  p2p::signed_msg_type signed_msg_type;
  validator_set val_set;

  //  std::mutex mtx;
  // bit_array
  p2p::vote_message votes;
  int64_t sum;
  p2p::block_id maj23;
  std::map<std::string, block_votes> votes_by_block;
  std::map<P2PID, p2p::block_id> peer_maj23s;
};

} // namespace noir::consensus
