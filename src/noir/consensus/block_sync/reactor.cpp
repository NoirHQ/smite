// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/overloaded.h>
#include <noir/consensus/block_sync/reactor.h>

#include <utility>

namespace noir::consensus::block_sync {

void reactor::process_peer_update(plugin_interface::peer_status_info_ptr info) {
  dlog(fmt::format(
    "[bs_reactor] peer update: peer_id={}, status={}", info->peer_id, p2p::peer_status_to_str(info->status)));
  switch (info->status) {
  case p2p::peer_status::up: {
    pool->transmit_new_envelope("", info->peer_id, status_response{store->height(), store->base()});
    break;
  }
  case p2p::peer_status::down: {
    pool->remove_peer(info->peer_id);
  }
  default:
    break;
  }
}

void reactor::process_peer_msg(p2p::envelope_ptr info) {
  auto from = info->from;

  datastream<char> ds(info->message.data(), info->message.size());
  p2p::bs_reactor_message msg;
  ds >> msg;

  dlog(fmt::format("received message={} from='{}'", msg.index(), from));

  std::visit(
    overloaded{///
      /*********************************************************************************************************/
      [this, &from](
        consensus::block_request& msg) { respond_to_peer(std::make_shared<consensus::block_request>(msg), from); },
      [this, &from](consensus::block_response& msg) {
        auto block_ = std::make_shared<consensus::block>();
        datastream<char> ds_block(msg.block_.data(), msg.block_.size());
        ds_block >> block_;
        pool->add_block(from, block_, msg.block_.size());
      },
      [this, &from](consensus::status_request& msg) {
        pool->transmit_new_envelope("", from, status_response{store->height(), store->base()});
      },
      [this, &from](consensus::status_response& msg) { pool->set_peer_range(from, msg.base, msg.height); },
      [](consensus::no_block_response& msg) {
        dlog(fmt::format("peer does not have the requested block: height={}", msg.height));
      }},
    msg);
}

void reactor::request_routine() {
  ///< error_ch [done]
  ///< request_ch TODO: should be simple

  pool->strand->post([this]() {
    if (!pool->is_running)
      return;
    auto peer_ids = pool->get_peer_ids();
    for (const auto& peer_id : peer_ids)
      pool->transmit_new_envelope("", peer_id, status_request{});
    std::this_thread::sleep_for(status_update_interval);
    request_routine();
  });
}

void reactor::pool_routine() {
  request_routine();
}

void reactor::respond_to_peer(std::shared_ptr<consensus::block_request> msg, const std::string& peer_id) {
  block block_;
  if (store->load_block(msg->height, block_)) {
    auto response = block_response{};
    response.block_ = encode(block_); // block is serialized when put into a response
    pool->transmit_new_envelope("", peer_id, response);
    return;
  }
  ilog("peer requested a block we do not have: peer=${peer_id} height=${h}", ("peer_id", peer_id)("h", msg->height));
  pool->transmit_new_envelope("", peer_id, no_block_response{msg->height});
}

} // namespace noir::consensus::block_sync
