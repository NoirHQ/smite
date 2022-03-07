// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/overloaded.h>
#include <noir/consensus/block_sync/reactor.h>
#include <noir/core/codec.h>

#include <utility>

namespace noir::consensus::block_sync {

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
        transmit_new_envelope("", from, status_response{store->height(), store->base()});
      },
      [this, &from](consensus::status_response& msg) { pool->set_peer_range(from, msg.base, msg.height); },
      [](consensus::no_block_response& msg) {
        dlog(fmt::format("peer does not have the requested block: height={}", msg.height));
      }},
    msg);
}

void reactor::transmit_new_envelope(
  const std::string& from, const std::string& to, const p2p::bs_reactor_message& msg, bool broadcast, int priority) {
  dlog(fmt::format("transmitting a new envelope: id=BlockSync, to={}, msg_type={}", to, msg.index(), broadcast));
  auto new_env = std::make_shared<p2p::envelope>();
  new_env->from = from;
  new_env->to = to;
  new_env->broadcast = false; // always false for block_sync
  new_env->id = p2p::BlockSync;

  const uint32_t payload_size = encode_size(msg);
  new_env->message.resize(payload_size);
  datastream<char> ds(new_env->message.data(), payload_size);
  ds << msg;

  xmt_mq_channel.publish(priority, new_env);
}

} // namespace noir::consensus::block_sync
