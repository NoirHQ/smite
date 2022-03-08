// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/block_sync/block_pool.h>
#include <noir/core/codec.h>

namespace noir::consensus::block_sync {

void block_pool::remove_timed_out_peers() {
  std::lock_guard<std::mutex> g(mtx);

  for (auto const& [k, peer] : peers) {
    // Check if peer timed out
    if (!peer->did_timeout && peer->num_pending > 0) {
      // auto cur_rate = peer->recv_monitor // TODO : how to measure receive rate (apply limit rate?)
    }
    if (peer->did_timeout)
      remove_peer(peer->id);
  }
}

void block_pool::remove_peer(std::string peer_id) {
  for (auto const& [k, requester] : requesters) {
    if (requester->get_peer_id() == peer_id)
      requester->redo(peer_id);
  }

  auto it = peers.find(peer_id);
  if (it != peers.end()) {
    if (it->second->timeout != nullptr)
      it->second->timeout->cancel();
    if (it->second->height == max_peer_height)
      update_max_peer_height();
    peers.erase(it);
  }
}

void block_pool::update_max_peer_height() {
  int64_t max{0};
  for (auto const& [k, peer] : peers) {
    if (peer->height > max)
      max = peer->height;
  }
  max_peer_height = max;
}

std::shared_ptr<bp_peer> block_pool::pick_incr_available_peer(int64_t height) {
  std::lock_guard<std::mutex> g(mtx);
  for (auto const& [k, peer] : peers) {
    if (peer->did_timeout) {
      remove_peer(peer->id);
      continue;
    }
    if (peer->num_pending >= max_pending_requests_per_peer)
      continue;
    if (height < peer->base || height > peer->height)
      continue;
    peer->incr_pending();
    return peer;
  }
  return {};
}

void block_pool::add_block(std::string peer_id, std::shared_ptr<consensus::block> block_, int block_size) {
  std::lock_guard<std::mutex> g(mtx);
  auto requester = requesters.find(block_->header.height);
  if (requester == requesters.end()) {
    elog("peer sent us a block we didn't expect");
    auto diff = height - block_->header.height;
    if (diff < 0)
      diff *= -1;
    if (diff > max_diff_btn_curr_and_recv_block_height)
      send_error("peer sent us a block we didn't expect with a height too far ahead", peer_id);
    return;
  }
  if (requester->second->set_block(block_, peer_id)) {
    num_pending -= 1;
    auto peer = peers.find(peer_id);
    if (peer != peers.end())
      peer->second->decr_pending(block_size);
  } else {
    std::string err("requester is different or block already exists");
    elog(err);
    send_error(err, peer_id);
  }
}

void block_pool::set_peer_range(std::string peer_id, int64_t base, int64_t height) {
  std::lock_guard<std::mutex> g(mtx);
  auto peer = peers.find(peer_id);
  if (peer != peers.end()) {
    peer->second->base = base;
    peer->second->height = height;
  } else {
    auto new_peer = bp_peer::new_bp_peer(shared_from_this(), peer_id, base, height, thread_pool->get_executor());
    peers[peer_id] = new_peer;
  }
  if (height > max_peer_height)
    max_peer_height = height;
}

void block_pool::send_request(int64_t height, std::string peer_id) {
  if (!is_running)
    return;
  transmit_new_envelope("", peer_id, noir::consensus::block_request{height});
}

void block_pool::send_error(std::string err, std::string peer_id) {
  if (!is_running)
    return;
  appbase::app().get_method<plugin_interface::methods::send_error_to_peer>()(peer_id, err);
}

void block_pool::transmit_new_envelope(
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
