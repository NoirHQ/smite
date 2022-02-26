// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/common/types.h>
#include <noir/consensus/bit_array.h>
#include <noir/consensus/merkle/proof.h>
#include <noir/p2p/types.h>

#include <variant>

namespace noir::p2p {

struct handshake_message {
  uint16_t network_version = 0; ///< incremental value above a computed base
  //  chain_id_type chain_id; ///< used to identify chain bytes32 node_id; ///< used to identify peers and prevent
  //  self-connect
  bytes32 node_id; ///< used to identify peers and prevent self-connect
  tstamp time{0};
  std::string p2p_address;
  uint32_t head_num = 0;
  block_id_type head_id;
  //  string os;
  //  string agent;
  int16_t generation = 0;
};

struct time_message {
  tstamp org{0}; //!< origin timestamp
  tstamp rec{0}; //!< receive timestamp
  tstamp xmt{0}; //!< transmit timestamp
  mutable tstamp dst{0}; //!< destination timestamp
};

enum signed_msg_type {
  Unknown = 0,
  Prevote = 1,
  Precommit = 2,
  Proposal = 32
};

inline bool is_vote_type_valid(signed_msg_type type) {
  if (type == Prevote || type == Precommit)
    return true;
  return false;
}

struct part_set_header {
  uint32_t total;
  bytes hash;

  bool operator==(const part_set_header& rhs) const {
    return (total == rhs.total) && (hash == rhs.hash);
  }

  bool is_zero() {
    return (total == 0) && (hash.empty());
  }
};

struct block_id {
  bytes hash;
  part_set_header parts;

  bool operator==(const block_id& rhs) const {
    return (hash == rhs.hash) && (parts == rhs.parts);
  }

  bool is_complete() const {
    return (parts.total > 0);
  }

  bool is_zero() {
    return (hash.empty() && parts.is_zero());
  }

  std::string key() {
    // returns a machine-readable string representation of the block_id
    // todo
    return to_hex(hash);
  }
};

struct vote_extension_to_sign {
  bytes add_data_to_sign;
};

struct vote_extension {
  bytes app_data_to_sign;
  bytes app_data_self_authenticating;

  vote_extension_to_sign to_sign() {
    return vote_extension_to_sign{app_data_to_sign};
  }
};

///< consensus message starts
struct new_round_step_message {
  int64_t height;
  int32_t round;
  round_step_type step;
  int64_t seconds_since_start_time;
  int32_t last_commit_round;
};

struct new_valid_block_message {
  int64_t height;
  int32_t round;
  part_set_header block_part_set_header;
  std::shared_ptr<consensus::bit_array> block_parts;
  bool is_commit;
};

struct proposal_message {
  signed_msg_type type;
  int64_t height;
  int32_t round;
  int32_t pol_round;
  block_id block_id_;
  tstamp timestamp{0};
  bytes signature;
};

struct proposal_pol_message {
  int64_t height;
  int32_t proposal_pol_round;
  consensus::bit_array proposal_pol;
};

struct block_part_message {
  int64_t height;
  int32_t round;
  uint32_t index;
  bytes bytes_;
  consensus::merkle::proof proof;
};

struct vote_message {
  signed_msg_type type;
  int64_t height;
  int32_t round;
  block_id block_id_;
  tstamp timestamp;
  bytes validator_address;
  int32_t validator_index;
  bytes signature;
  vote_extension vote_extension_;
};

struct has_vote_message {
  int64_t height;
  int32_t round;
  signed_msg_type type;
  int32_t index;
};

struct vote_set_maj23_message {
  int64_t height;
  int32_t round;
  signed_msg_type type;
  block_id block_id_;
};

struct vote_set_bits_message {
  int64_t height;
  int32_t round;
  signed_msg_type type;
  block_id block_id_;
  consensus::bit_array votes;
};
///< consensus message ends

enum go_away_reason {
  no_reason, ///< no reason to go away
  self, ///< the connection is to itself
  duplicate, ///< the connection is redundant
  wrong_chain, ///< the peer's chain id doesn't match
  unlinkable, ///< the peer sent a block we couldn't use
  validation, ///< the peer sent a block that failed validation
  benign_other, ///< reasons such as a timeout. not fatal but warrant resetting
  fatal_other ///< a catch-all for errors we don't have discriminated
};

constexpr auto reason_str(go_away_reason rsn) {
  switch (rsn) {
  case no_reason:
    return "no reason";
  case self:
    return "self connect";
  case duplicate:
    return "duplicate";
  case wrong_chain:
    return "wrong chain";
  case unlinkable:
    return "unlinkable block received";
  case validation:
    return "invalid block";
  case fatal_other:
    return "some other failure";
  case benign_other:
    return "some other non-fatal condition, possibly unknown block";
  default:
    return "some crazy reason";
  }
}

struct go_away_message {
  go_away_message(go_away_reason r = no_reason): reason(r), node_id() {}

  go_away_reason reason{no_reason};
  bytes32 node_id; ///< for duplicate notification
};

using net_message =
  std::variant<handshake_message, go_away_message, time_message, proposal_message, block_part_message, vote_message>;
struct p2p_msg_info {
  net_message msg;
  node_id peer_id;
  bool broadcast;
};
using p2p_msg_info_ptr = std::shared_ptr<p2p_msg_info>;

using consensus_message = std::variant<proposal_message, block_part_message, vote_message>;
struct msg_info {
  consensus_message msg;
  node_id peer_id;
};
using msg_info_ptr = std::shared_ptr<msg_info>;

} // namespace noir::p2p

NOIR_FOR_EACH_FIELD(
  noir::p2p::handshake_message, network_version, node_id, time, p2p_address, head_num, head_id, generation)
NOIR_FOR_EACH_FIELD(noir::p2p::go_away_message, reason, node_id)
NOIR_FOR_EACH_FIELD(noir::p2p::time_message, org, rec, xmt, dst)
NOIR_FOR_EACH_FIELD(noir::p2p::block_part_message, height, round, index, bytes_ /* TODO: (proof) */)
NOIR_FOR_EACH_FIELD(noir::p2p::block_id, hash, parts)
NOIR_FOR_EACH_FIELD(noir::p2p::part_set_header, total, hash)
NOIR_FOR_EACH_FIELD(noir::p2p::vote_extension, app_data_to_sign, app_data_self_authenticating)
NOIR_FOR_EACH_FIELD(noir::p2p::vote_message, type, height, round, timestamp /*, vote_extension_ */)
NOIR_FOR_EACH_FIELD(noir::p2p::proposal_message, type, height, round, pol_round, timestamp)
