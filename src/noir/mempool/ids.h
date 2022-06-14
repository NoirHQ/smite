// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types/node_id.h>
#include <map>
#include <set>
#include <shared_mutex>

namespace noir::mempool {

constexpr uint16_t max_active_ids = std::numeric_limits<uint16_t>::max();
constexpr uint16_t unknown_peer_id = 0;

class MempoolIDs {
public:
  void reserve_for_peer(const consensus::node_id& peer_id);
  void reclaim(const consensus::node_id& peer_id);
  uint16_t get_for_peer(const consensus::node_id& peer_id);
  uint16_t next_peer_id();

private:
  std::shared_mutex mtx;
  std::map<consensus::node_id, uint16_t> peer_map;
  uint16_t next_id = 1;
  std::set<uint16_t> active_ids = {unknown_peer_id};
};

} // namespace noir::mempool
