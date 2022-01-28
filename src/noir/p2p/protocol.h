// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/p2p/types.h>

namespace noir::p2p {

using namespace fc;

struct handshake_message {
  uint16_t network_version = 0; ///< incremental value above a computed base
  //  chain_id_type chain_id; ///< used to identify chain fc::sha256 node_id; ///< used to identify peers and prevent
  //  self-connect
  fc::sha256 node_id; ///< used to identify peers and prevent self-connect
  //  chain::public_key_type key; ///< authentication key; may be a producer or peer key, or empty
  tstamp time{0};
  fc::sha256 token; ///< digest of time to prove we own the private key of the key above
  //  chain::signature_type sig; ///< signature for the digest
  string p2p_address;
  uint32_t last_irreversible_block_num = 0;
  block_id_type last_irreversible_block_id;
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

struct part_set_header {
  uint32_t total;
  sha256 hash;
};

struct block_id {
  sha256 hash;
  part_set_header parts;
};

struct vote_extension {
  bytes app_data_to_sign;
  bytes app_data_self_authenticating;
};

struct proposal_message {
  signed_msg_type type;
  int64_t height;
  int32_t round;
  int32_t pol_round;
  block_id my_block_id;
  tstamp timestamp{0};
  signature sig;
};

struct block_part_message {
  int64_t height;
  int32_t round;
  uint32_t index;
  bytes bs;
  bytes proof;
};

struct vote_message {
  signed_msg_type type;
  int64_t height;
  int32_t round;
  block_id my_block_id;
  tstamp timestamp;
  bytes validator_address;
  int32_t validator_index;
  signature sig;
  vote_extension my_vote_extension;
};

enum go_away_reason {
  no_reason, ///< no reason to go away
  self, ///< the connection is to itself
  duplicate, ///< the connection is redundant
  wrong_chain, ///< the peer's chain id doesn't match
  wrong_version, ///< the peer's network version doesn't match
  forked, ///< the peer's irreversible blocks are different
  unlinkable, ///< the peer sent a block we couldn't use
  bad_transaction, ///< the peer sent a transaction that failed verification
  validation, ///< the peer sent a block that failed validation
  benign_other, ///< reasons such as a timeout. not fatal but warrant resetting
  fatal_other, ///< a catch-all for errors we don't have discriminated
  authentication ///< peer failed authenicatio
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
  case wrong_version:
    return "wrong version";
  case forked:
    return "chain is forked";
  case unlinkable:
    return "unlinkable block received";
  case bad_transaction:
    return "bad transaction";
  case validation:
    return "invalid block";
  case authentication:
    return "authentication failure";
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
  fc::sha256 node_id; ///< for duplicate notification
};

using net_message =
  std::variant<handshake_message, go_away_message, time_message, proposal_message, block_part_message, vote_message>;

} // namespace noir::p2p

FC_REFLECT(noir::p2p::handshake_message,
  (network_version)(node_id)(time)(token)(p2p_address)(last_irreversible_block_num)(last_irreversible_block_id)(head_num)(head_id)(generation))
FC_REFLECT(noir::p2p::go_away_message, (reason)(node_id))
FC_REFLECT(noir::p2p::time_message, (org)(rec)(xmt)(dst))
FC_REFLECT(noir::p2p::proposal_message, (type)(height)(round)(pol_round)(my_block_id)(timestamp)(sig))
FC_REFLECT(noir::p2p::block_part_message, (height)(round)(index)(bs)(proof))
FC_REFLECT(noir::p2p::vote_message,
  (type)(height)(round)(my_block_id)(timestamp)(validator_address)(validator_index)(sig)(my_vote_extension))
FC_REFLECT(noir::p2p::block_id, (hash)(parts))
FC_REFLECT(noir::p2p::part_set_header, (total)(hash))
FC_REFLECT(noir::p2p::vote_extension, (app_data_to_sign)(app_data_self_authenticating))
