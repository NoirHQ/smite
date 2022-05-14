// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/proto3.h>
#include <noir/consensus/evidence/evidence_pool.h>

namespace noir::consensus::ev {

result<void> evidence_pool::mark_evidence_as_committed(evidence_list& evs, int64_t height) {
  std::set<std::string> block_evidence_map;
  std::vector<bytes> batch_delete;

  for (auto& ev : evs.list) {
    if (is_pending(ev)) {
      batch_delete.push_back(key_pending(ev));
      block_evidence_map.insert(ev_map_key(ev));
    }
    auto key = key_committed(ev);
    auto ev_bytes = noir::codec::proto3::encode(height);
    evidence_store->write_from_bytes(key, ev_bytes); // TODO: check
    dlog(fmt::format("marked evidence as committed: evidence{}", ev->get_string()));
  }

  if (block_evidence_map.empty())
    return {};
  evidence_store->erase(batch_delete);
  remove_evidence_from_list(block_evidence_map);
  std::atomic_fetch_sub_explicit(&evidence_size, block_evidence_map.size(), std::memory_order_relaxed);
}

result<std::pair<std::vector<std::shared_ptr<evidence>>, int64_t>> evidence_pool::list_evidence(
  prefix prefix_key, int64_t max_bytes) {
  // TODO: requires iterate_prefix to continue
  return {};
}

std::pair<int64_t, tstamp> evidence_pool::remove_expired_pending_evidence() {
  std::vector<bytes> batch_delete;

  auto [height, time, block_evidence_map] = batch_expired_pending_evidence(batch_delete);
  if (block_evidence_map.empty())
    return {height, time};
  dlog("removing expired evidence");
  evidence_store->erase(batch_delete);
  remove_evidence_from_list(block_evidence_map);
  std::atomic_fetch_sub_explicit(&evidence_size, block_evidence_map.size(), std::memory_order_relaxed);
  return {height, time};
}

std::tuple<int64_t, tstamp, std::set<std::string>> evidence_pool::batch_expired_pending_evidence(std::vector<bytes>&) {
  std::set<std::string> block_evidence_map;
  // TODO: requires iterate_prefix to continue
  return {};
}

result<void> evidence_pool::verify(std::shared_ptr<evidence> ev) {}

result<void> evidence_pool::verify_light_client_attack(std::shared_ptr<light_client_attack_evidence> ev,
  std::shared_ptr<signed_header> common_header,
  std::shared_ptr<signed_header> trusted_header,
  std::shared_ptr<validator_set> common_vals) {}

result<void> evidence_pool::verify_duplicate_vote(
  std::shared_ptr<duplicate_vote_evidence> ev, std::string chain_id, std::shared_ptr<validator_set> val_set) {}

} // namespace noir::consensus::ev
