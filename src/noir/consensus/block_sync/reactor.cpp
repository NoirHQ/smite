// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/protobuf.h>
#include <noir/common/overloaded.h>
#include <noir/consensus/block_sync/reactor.h>
#include <noir/consensus/types/validation.h>
#include <tendermint/blocksync/types.pb.h>

#include <utility>

namespace noir::consensus::block_sync {

void reactor::process_peer_update(plugin_interface::peer_status_info_ptr info) {
  dlog(fmt::format(
    "[bs_reactor] peer update: peer_id={}, status={}", info->peer_id, p2p::peer_status_to_str(info->status)));
  switch (info->status) {
  case p2p::peer_status::up: {
    pool->transmit_new_envelope(info->peer_id, status_response{store->height(), store->base()});
    break;
  }
  case p2p::peer_status::down: {
    pool->remove_peer(info->peer_id);
    break;
  }
  default:
    break;
  }
}

void reactor::process_peer_msg(p2p::envelope_ptr info) {
  auto from = info->from;
  auto to = info->broadcast ? "all" : info->to;

  // Use protobuf
  p2p::bs_reactor_message bs_msg;
  ::tendermint::blocksync::Message pb_msg;
  pb_msg.ParseFromArray(info->message.data(), info->message.size());
  if (!pb_msg.IsInitialized()) {
    elog("unable to parse envelop message: size=${size}", ("size", info->message.size()));
    return;
  }
  switch (pb_msg.sum_case()) {
  case tendermint::blocksync::Message::kBlockRequest: {
    const auto& m = pb_msg.block_request();
    bs_msg = block_request{.height = m.height()};
  } break;
  case tendermint::blocksync::Message::kNoBlockResponse: {
    const auto& m = pb_msg.no_block_response();
    bs_msg = no_block_response{.height = m.height()};
  } break;
  case tendermint::blocksync::Message::kBlockResponse: {
    const auto& m = pb_msg.block_response();
    bs_msg = block_response{.block_ = codec::protobuf::encode(m.block())};
  } break;
  case tendermint::blocksync::Message::kStatusRequest: {
    // const auto& m = pb_msg.status_request();
    bs_msg = status_request{};
  } break;
  case tendermint::blocksync::Message::kStatusResponse: {
    const auto& m = pb_msg.status_response();
    bs_msg = status_response{.height = m.height(), .base = m.base()};
  } break;
  default:
    elog("block_sync receive failed: unable to determine message type type=${type}", ("type", pb_msg.sum_case()));
    return;
  }

  dlog(fmt::format("[bs_reactor] recv msg. from={}, to={}, type={}", from, to, bs_msg.index()));

  std::visit(
    overloaded{///
      /*********************************************************************************************************/
      [this, &from](
        consensus::block_request& msg) { respond_to_peer(std::make_shared<consensus::block_request>(msg), from); },
      [this, &from](consensus::block_response& msg) {
        try {
          auto pb_block = codec::protobuf::decode<::tendermint::types::Block>(msg.block_);
          pool->add_block(from, block::from_proto(pb_block), msg.block_.size());
        } catch (std::exception&) {
          wlog(fmt::format("unable to parse received block_response: size={}", msg.block_.size()));
        }
      },
      [this, &from](consensus::status_request& msg) {
        pool->transmit_new_envelope(from, status_response{store->height(), store->base()});
      },
      [this, &from](consensus::status_response& msg) {
        if (!block_sync)
          return;
        pool->set_peer_range(from, msg.base, msg.height);
      },
      [](consensus::no_block_response& msg) {
        dlog(fmt::format("peer does not have the requested block: height={}", msg.height));
      }},
    bs_msg);
}

void reactor::request_routine() {
  thread_pool->get_executor().post([this]() {
    while (true) {
      if (!pool->is_running)
        return;
      auto peer_ids = pool->get_peer_ids();
      for (const auto& peer_id : peer_ids)
        pool->transmit_new_envelope(peer_id, status_request{});
      std::this_thread::sleep_for(status_update_interval);
    }
  });
}

void reactor::pool_routine(bool state_synced) {
  request_routine();
  try_sync_ticker();
  switch_to_consensus_ticker();
}

void reactor::try_sync_ticker() {
  thread_pool->get_executor().post([this]() {
    auto chain_id = initial_state.chain_id;
    latest_state = initial_state;
    blocks_synced = 0;

    while (true) {
      if (!pool->is_running)
        return;

      std::this_thread::sleep_for(try_sync_interval);
      auto [first, second] = pool->peek_two_blocks();
      if (!first || !second)
        continue;

      auto first_parts = first->make_part_set(block_part_size_bytes);
      auto first_part_set_header = first_parts->header();
      auto first_id = p2p::block_id{first->get_hash(), first_part_set_header};

      // Verify the first block using the second's commit
      if (auto err = verify_commit_light(chain_id, latest_state.validators, first_id, first->header.height,
            std::make_shared<commit>(*second->last_commit));
          err.has_value()) {
        elog(fmt::format("invalid last commit: height={} err={}", first->header.height, err.value()));

        // We already removed peer request, but we need to clean up the rest
        auto peer_id1 = pool->redo_request(first->header.height);
        pool->send_error(err.value(), peer_id1);
        auto peer_id2 = pool->redo_request(second->header.height);
        pool->send_error(err.value(), peer_id2);
      } else {
        pool->pop_request();

        store->save_block(*first, *first_parts, *second->last_commit);

        auto new_state = block_exec->apply_block(latest_state, first_id, first);
        if (!new_state.has_value()) {
          check(false, fmt::format("Panic: failed to process committed block: height={}", first->header.height));
        }
        latest_state = new_state.value();

        blocks_synced++;
      }
    }
  });
}

void reactor::switch_to_consensus_ticker() {
  thread_pool->get_executor().post([this]() {
    while (true) {
      auto [height, num_pending, len_requesters] = pool->get_status();
      auto last_advance = pool->last_advance;
      dlog(fmt::format("consensus_ticker: num_pending={} total={} height={}", num_pending, len_requesters, height));

      if (pool->is_caught_up()) {
        ilog(fmt::format("switching to consensus reactor: height={}", height));
      } else if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::microseconds(
                   (get_time() - last_advance))) > sync_timeout) { // TODO: check if this is right
        elog(fmt::format("no progress since last advance: last_advance={}", last_advance));
      } else {
        ilog(fmt::format("not caught up yet: height={} max_peer_height={} timeout_in={}", height, pool->max_peer_height,
          sync_timeout.count() - (get_time() - last_advance)));
        std::this_thread::sleep_for(switch_to_consensus_interval);
        continue;
      }
      break;
    }

    /// Let's switch to consensus

    bool expected = true;
    if (block_sync.compare_exchange_strong(expected, false)) {
      pool->on_stop();
    }

    if (callback_switch_to_cs_sync)
      callback_switch_to_cs_sync(latest_state, blocks_synced > 0);

    ilog("EXITING switch_to_consensus_ticker");
  });
}

void reactor::respond_to_peer(std::shared_ptr<consensus::block_request> msg, const std::string& peer_id) {
  block block_;
  if (store->load_block(msg->height, block_)) {
    auto response = block_response{};
    response.block_ = codec::protobuf::encode(*block::to_proto(block_)); // serialized when put into a response
    pool->transmit_new_envelope(peer_id, response);
    return;
  }
  ilog("peer requested a block we do not have: peer=${peer_id} height=${h}", ("peer_id", peer_id)("h", msg->height));
  pool->transmit_new_envelope(peer_id, no_block_response{msg->height});
}

} // namespace noir::consensus::block_sync
