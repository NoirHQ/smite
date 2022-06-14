// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/mempool/ids.h>

namespace noir::mempool {

void MempoolIDs::reserve_for_peer(const consensus::node_id& peer_id) {
  std::unique_lock g{mtx};

  auto cur_id = next_peer_id();
  peer_map[peer_id] = cur_id;
  active_ids.insert(cur_id);
}

void MempoolIDs::reclaim(const consensus::node_id& peer_id) {
  std::unique_lock g{mtx};

  auto remove_id = peer_map.find(peer_id);
  if (remove_id != peer_map.end()) {
    active_ids.erase(remove_id->second);
    peer_map.erase(remove_id);
  }
}

uint16_t MempoolIDs::get_for_peer(const consensus::node_id& peer_id) {
  std::shared_lock g{mtx};
  // XXX: check what happens map doesn't have the key in golang map
  return peer_map.at(peer_id);
}

uint16_t MempoolIDs::next_peer_id() {
  if (active_ids.size() == max_active_ids) {
    throw std::runtime_error(
      fmt::format("node has maximum {:d} active IDs and wanted to get one more", max_active_ids));
  }

  while (active_ids.contains(next_id)) {
    next_id++;
  }

  auto cur_id = next_id;
  next_id++;

  return cur_id;
}

} // namespace noir::mempool
