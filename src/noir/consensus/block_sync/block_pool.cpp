// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/block_sync/block_pool.h>

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

} // namespace noir::consensus::block_sync