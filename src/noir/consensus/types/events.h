// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>
#include <noir/consensus/common.h>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/round_state.h>

namespace noir::consensus::events {

// Reserved event types (alphabetically sorted).
enum class event_value {
  // Block level events for mass consumption by users.
  // These events are triggered from the state package,
  // after a block has been committed.
  // These are also used by the tx indexer for async indexing.
  // All of this data can be fetched through the rpc.
  new_block,
  new_block_header,
  new_evidence,
  tx,
  validator_set_updates,

  // Internal consensus events.
  // These are used for testing the consensus state machine.
  // They can also be used to build real-time consensus visualizers.
  complete_proposal,
  // The BlockSyncStatus event will be emitted when the node switching
  // state sync mechanism between the consensus reactor and the blocksync reactor.
  block_sync_status,
  lock,
  new_round,
  new_round_step,
  polka,
  relock,
  state_sync_status,
  timeout_propose,
  timeout_wait,
  unlock,
  valid_block,
  vote,
  unknown,
};

static std::string string_from_event_value(event_value val) {
  switch (val) {
  case event_value::new_block: {
    return "NewBlock";
  }
  case event_value::new_block_header: {
    return "NewBlockHeader";
  }
  case event_value::new_evidence: {
    return "NewEvidence";
  }
  case event_value::tx: {
    return "Tx";
  }
  case event_value::validator_set_updates: {
    return "ValidatorSetUpdates";
  }
  case event_value::complete_proposal: {
    return "CompleteProposal";
  }
  case event_value::block_sync_status: {
    return "BlockSyncStatus";
  }
  case event_value::lock: {
    return "Lock";
  }
  case event_value::new_round: {
    return "NewRound";
  }
  case event_value::new_round_step: {
    return "NewRoundStep";
  }
  case event_value::polka: {
    return "Polka";
  }
  case event_value::relock: {
    return "Relock";
  }
  case event_value::state_sync_status: {
    return "StateSyncStatus";
  }
  case event_value::timeout_propose: {
    return "TimeoutPropose";
  }
  case event_value::timeout_wait: {
    return "TimeoutWait";
  }
  case event_value::unlock: {
    return "Unlock";
  }
  case event_value::valid_block: {
    return "ValidBlock";
  }
  case event_value::vote: {
    return "Vote";
  }
  case event_value::unknown:
  default: {
    check(false, "invalid event_value: {}", static_cast<int>(val));
  }
  }
  return ""; // should not be reached
}

// EventTypeKey is a reserved composite key for event name.
constexpr std::string_view event_type_key = "tm.event";
// TxHashKey is a reserved key, used to specify transaction's hash.
// see EventBus#PublishEventTx
constexpr std::string_view tx_hash_key = "tx.hash";
// TxHeightKey is a reserved key, used to specify transaction block's height.
// see EventBus#PublishEventTx
constexpr std::string_view tx_height_key = "tx.height";

// BlockHeightKey is a reserved key used for indexing BeginBlock and Endblock
// events.
constexpr std::string_view block_height_key = "block.height";

constexpr std::string_view event_type_begin_block = "begin_block";
constexpr std::string_view event_type_end_block = "end_block";

// Pre-populated ABCI Tendermint-reserved events
template<event_value e>
::tendermint::abci::Event prepopulated_event() {
  ::tendermint::abci::Event ev;
  ev.set_type("tm");
  auto attrs = ev.mutable_attributes();
  auto attr = attrs->Add();
  attr->set_key("event");
  attr->set_value(string_from_event_value(e));
  return ev;
}

// Most event messages are basic types (a block, a transaction)
// but some (an input to a call tx or a receive) are more exotic
struct event_data_new_block {
  noir::consensus::block block;
  p2p::block_id block_id;
  tendermint::abci::ResponseBeginBlock result_begin_block;
  tendermint::abci::ResponseEndBlock result_end_block;
};

struct event_data_new_block_header {
  block_header header;
  int64_t num_txs;
  tendermint::abci::ResponseBeginBlock result_begin_block;
  tendermint::abci::ResponseEndBlock result_end_block;
};

struct event_data_new_evidence {
  std::shared_ptr<noir::consensus::evidence> ev{};
  int64_t height{};
};

struct event_data_tx {
  tendermint::abci::TxResult tx_result;
};

struct event_data_round_state {
  int64_t height;
  int32_t round;
  std::string step;

  event_data_round_state() = default;
  event_data_round_state(const event_data_round_state&) = default;
  event_data_round_state(event_data_round_state&&) = default;
  explicit event_data_round_state(const round_state& rs)
    : height(rs.height), round(rs.round), step(p2p::round_step_to_str(rs.step)) {}
  event_data_round_state& operator=(const event_data_round_state& other) = default;
  event_data_round_state& operator=(event_data_round_state&& other) = default;
};

struct validator_info {
  Bytes address;
  int32_t index;
};

struct event_data_new_round {
  int64_t height;
  int32_t round;
  std::string step;

  validator_info proposer;

  event_data_new_round() = default;
  event_data_new_round(const event_data_new_round&) = default;
  event_data_new_round(event_data_new_round&&) = default;
  explicit event_data_new_round(const round_state& rs)
    : height(rs.height), round(rs.round), step(p2p::round_step_to_str(rs.step)) {
    auto addr = rs.validators->get_proposer()->address;
    auto idx = rs.validators->get_index_by_address(addr);
    proposer = validator_info{
      .address = addr,
      .index = idx,
    };
  }
  event_data_new_round& operator=(const event_data_new_round& other) = default;
  event_data_new_round& operator=(event_data_new_round&& other) = default;
};

struct event_data_complete_proposal {
  int64_t height;
  int32_t round;
  std::string step;

  noir::p2p::block_id block_id;

  event_data_complete_proposal() = default;
  event_data_complete_proposal(const event_data_complete_proposal&) = default;
  event_data_complete_proposal(event_data_complete_proposal&&) = default;
  explicit event_data_complete_proposal(const round_state& rs)
    : height(rs.height),
      round(rs.round),
      step(p2p::round_step_to_str(rs.step)),
      block_id(noir::p2p::block_id{.hash = rs.proposal_block->get_hash(), .parts = rs.proposal_block_parts->header()}) {
  }
  event_data_complete_proposal& operator=(const event_data_complete_proposal& other) = default;
  event_data_complete_proposal& operator=(event_data_complete_proposal&& other) = default;
};

struct event_data_vote {
  std::shared_ptr<noir::consensus::vote> vote;
};

struct event_data_string {
  std::string string;
};

struct event_data_validator_set_updates {
  std::vector<validator> validator_updates;
};

/// EventDataBlockSyncStatus shows the fastsync status and the
/// height when the node state sync mechanism changes.
struct event_data_block_sync_status {
  bool complete;
  int64_t height;
};

/// EventDataStateSyncStatus shows the statesync status and the
/// height when the node state sync mechanism changes.
struct event_data_state_sync_status {
  bool complete;
  int64_t height;
};

/// TMEventData implements events.EventData.
using tm_event_data = std::variant<event_data_new_block,
  event_data_new_block_header,
  event_data_new_evidence,
  event_data_tx,
  event_data_round_state,
  event_data_new_round,
  event_data_complete_proposal,
  event_data_vote,
  event_data_string,
  event_data_validator_set_updates,
  event_data_block_sync_status,
  event_data_state_sync_status>;

} // namespace noir::consensus::events

NOIR_REFLECT(noir::consensus::events::event_data_new_block, block, block_id, result_begin_block, result_end_block);
NOIR_REFLECT(
  noir::consensus::events::event_data_new_block_header, header, num_txs, result_begin_block, result_end_block);
NOIR_REFLECT(noir::consensus::events::event_data_new_evidence, /*evidence, */ height);
NOIR_REFLECT(noir::consensus::events::event_data_tx, tx_result);
NOIR_REFLECT(noir::consensus::events::event_data_round_state, height, round, step);
NOIR_REFLECT(noir::consensus::events::validator_info, address, index);
NOIR_REFLECT(noir::consensus::events::event_data_new_round, height, round, step, proposer);
NOIR_REFLECT(noir::consensus::events::event_data_complete_proposal, height, round, step, block_id);
NOIR_REFLECT(noir::consensus::events::event_data_vote, vote);
NOIR_REFLECT(noir::consensus::events::event_data_string, string);
NOIR_REFLECT(noir::consensus::events::event_data_validator_set_updates, validator_updates);
NOIR_REFLECT(noir::consensus::events::event_data_block_sync_status, complete, height);
NOIR_REFLECT(noir::consensus::events::event_data_state_sync_status, complete, height);
