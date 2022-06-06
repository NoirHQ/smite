// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/consensus/state.h>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/state_store.h>
#include <noir/consensus/types/evidence.h>
#include <noir/db/rocks_session.h>
#include <noir/db/session.h>

namespace noir::consensus::ev {

using db_session_type = noir::db::session::session<noir::db::session::rocksdb_t>;

enum class prefix : int64_t {
  prefix_committed = 9,
  prefix_pending = 10,
};

struct duplicate_vote_set {
  std::shared_ptr<vote> vote_a{};
  std::shared_ptr<vote> vote_b{};
};

struct evidence_pool {
  std::shared_ptr<db_session_type> evidence_store{};
  std::vector<std::shared_ptr<evidence>> ev_list{};
  std::atomic<uint32_t> evidence_size{};

  std::shared_ptr<noir::consensus::db_store> state_db{};
  std::shared_ptr<noir::consensus::block_store> block_store{};

  std::mutex mtx;
  std::unique_ptr<noir::consensus::state> state{};
  std::vector<duplicate_vote_set> consensus_buffer{};

  int64_t pruning_height{};
  tstamp pruning_time{};

  static Result<std::shared_ptr<evidence_pool>> new_pool(std::shared_ptr<db_session_type> new_evidence_store,
    std::shared_ptr<noir::consensus::db_store> new_state_db,
    std::shared_ptr<noir::consensus::block_store> new_block_store) {
    auto ret = std::make_shared<evidence_pool>();
    ret->state = std::make_unique<noir::consensus::state>();
    if (!new_state_db->load(*ret->state))
      return Error::format("failed to load state");
    ret->evidence_store = std::move(new_evidence_store);
    ret->state_db = std::move(new_state_db);
    ret->block_store = std::move(new_block_store);

    auto [h, t] = ret->remove_expired_pending_evidence();
    ret->pruning_height = h;
    ret->pruning_time = t;
    auto ok = ret->list_evidence(prefix::prefix_pending, -1);
    if (!ok)
      return ok.error();
    auto [ev_list, _] = ok.value();

    ret->evidence_size.store(ev_list.size());
    for (auto& e : ev_list)
      ret->ev_list.emplace_back(e);
    return ret;
  }

  std::pair<std::vector<std::shared_ptr<evidence>>, int64_t> pending_evidence(int64_t max_bytes) {
    if (get_size() == 0)
      return {{}, 0};
    auto ok = list_evidence(prefix::prefix_pending, max_bytes);
    if (!ok) {
      elog(fmt::format("failed to retrieve pending evidence: {}", ok.error()));
      return {{}, 0}; // TODO : check
    }
    return ok.value();
  }

  void update(noir::consensus::state& new_state, evidence_list evs) {
    if (new_state.last_block_height <= state->last_block_height)
      check(false, fmt::format("failed evidence.update: new state has less or equal height than previous height"));
    dlog(fmt::format("updating evidence_pool: last_block_height={}", new_state.last_block_height));

    process_consensus_buffer(new_state);
    update_state(new_state);
    mark_evidence_as_committed(evs, new_state.last_block_height);

    if (get_size() > 0 && new_state.last_block_height > pruning_height && new_state.last_block_time > pruning_time)
      std::tie(pruning_height, pruning_time) = remove_expired_pending_evidence();
  }

  Result<void> add_evidence(std::shared_ptr<evidence> ev) {
    dlog("attempting to add evidence");
    if (is_pending(ev)) {
      dlog("evidence already pending; ignoring");
      return success();
    }
    if (is_committed(ev)) {
      dlog("evidence already committed; ignoring");
      return success();
    }
    if (auto ok = verify(ev); !ok)
      return ok.error();
    if (auto ok = add_pending_evidence(ev); !ok)
      return Error::format("failed to add evidence to pending list: {}", ok.error().message());
    ev_list.push_back(ev);
    ilog("verified new evidence of byzantine behavior");
    return success();
  }

  void report_conflicting_votes(std::shared_ptr<vote> vote_a, std::shared_ptr<vote> vote_b) {
    std::scoped_lock _(mtx);
    consensus_buffer.push_back(duplicate_vote_set{vote_a, vote_b});
  }

  Result<void> check_evidence(const evidence_list& evs) {
    std::vector<bytes> hashes(evs.list.size());
    int idx{0};
    for (auto& e : evs.list) {
      if (dynamic_cast<light_client_attack_evidence*>(e.get()) || !is_pending(e)) {
        if (is_committed(e))
          return Error::format("evidence was already committed");
        if (auto ok = verify(e); !ok)
          return ok.error();
        if (auto ok = add_pending_evidence(e); !ok)
          elog(fmt::format("failed to add evidence to pending list: {}", ok.error()));
        ilog("check evidence: verified evidence of byzantine behavior");
      }

      // Check for duplicate evidence; cache hashes for future
      hashes[idx] = e->get_hash();
      for (auto i = idx - 1; i >= 0; i--) {
        if (hashes[i] == hashes[idx])
          return Error::format("duplicate evidence");
      }
      idx++;
    }
    return success();
  }

  std::shared_ptr<evidence> evidence_front() {
    if (!ev_list.empty())
      return ev_list[0];
    return {};
  }

  uint32_t get_size() {
    return evidence_size.load();
  }

  noir::consensus::state get_state() {
    std::scoped_lock _(mtx);
    return *state;
  }

  bool is_expired(int64_t height, tstamp time) {
    auto params = get_state().consensus_params_.evidence;
    auto age_duration = get_state().last_block_time - time;
    auto age_num_blocks = get_state().last_block_height - height;
    return age_num_blocks > params.max_age_num_blocks && age_duration > params.max_age_duration;
  }

  bool is_committed(std::shared_ptr<evidence> ev) {
    auto key = key_committed(ev);
    return evidence_store->contains(noir::db::session::shared_bytes(key.data(), key.size()));
  }

  bool is_pending(std::shared_ptr<evidence> ev) {
    auto key = key_pending(ev);
    return evidence_store->contains(noir::db::session::shared_bytes(key.data(), key.size()));
  }

  Result<void> add_pending_evidence(std::shared_ptr<evidence> ev) {
    auto evpb = evidence::to_proto(*ev);
    if (!evpb)
      return evpb.error();
    bytes ev_bytes(evpb.value()->ByteSizeLong());
    evpb.value()->SerializeToArray(ev_bytes.data(), evpb.value()->ByteSizeLong()); // TODO: handle failure?
    auto key = key_pending(ev);
    evidence_store->write_from_bytes(key, ev_bytes); // TODO: check
    std::atomic_fetch_add_explicit(&evidence_size, 1, std::memory_order_relaxed);
    return success();
  }

  void mark_evidence_as_committed(evidence_list& evs, int64_t height);

  Result<std::pair<std::vector<std::shared_ptr<evidence>>, int64_t>> list_evidence(
    prefix prefix_key, int64_t max_bytes);

  std::pair<int64_t, tstamp> remove_expired_pending_evidence();

  std::tuple<int64_t, tstamp, std::set<std::string>> batch_expired_pending_evidence(std::vector<bytes>&);

  void remove_evidence_from_list(std::set<std::string>& block_evidence_map) {
    auto it = ev_list.begin();
    while (it != ev_list.end()) {
      if (block_evidence_map.find(ev_map_key(*it)) != block_evidence_map.end())
        it = ev_list.erase(it);
      else
        it++;
    }
  }

  void update_state(noir::consensus::state& new_state) {
    std::scoped_lock _(mtx);
    *state = new_state;
  }

  void process_consensus_buffer(noir::consensus::state& new_state) {
    std::scoped_lock _(mtx);
    for (auto& vote_set_ : consensus_buffer) {
      std::shared_ptr<duplicate_vote_evidence> dve;
      std::string err;
      if (vote_set_.vote_a->height == new_state.last_block_height) {
        if (auto ok = duplicate_vote_evidence::new_duplicate_vote_evidence(vote_set_.vote_a, vote_set_.vote_b,
              new_state.last_block_time, std::make_shared<validator_set>(new_state.last_validators));
            !ok) {
          err = ok.error().message();
        } else {
          dve = ok.value();
        }
      } else if (vote_set_.vote_a->height < new_state.last_block_height) {
        auto val_set = std::make_shared<validator_set>();
        if (!state_db->load_validators(vote_set_.vote_a->height, *val_set)) {
          elog(fmt::format("failed to load validator_set for conflicting votes: height={}", vote_set_.vote_a->height));
          continue;
        }
        block_meta b_meta;
        if (!block_store->load_block_meta(vote_set_.vote_a->height, b_meta)) {
          elog(fmt::format("failed to load block_meta for conflicting votes: height={}", vote_set_.vote_a->height));
          continue;
        }
        if (auto ok = duplicate_vote_evidence::new_duplicate_vote_evidence(
              vote_set_.vote_a, vote_set_.vote_b, b_meta.header.time, val_set);
            !ok) {
          err = ok.error().message();
        } else {
          dve = ok.value();
        }
      } else {
        elog(fmt::format("inbound duplicate votes from consensus are of a greater height than current state: height={}",
          vote_set_.vote_a->height));
      }

      if (!err.empty()) {
        elog(fmt::format("error in generating evidence from votes: {}", err));
        continue;
      }
      if (is_pending(dve)) {
        elog(fmt::format("evidence is already pending: evidence={}", dve->get_string()));
        continue;
      }
      if (is_committed(dve)) {
        elog(fmt::format("evidence is already committed: evidence={}", dve->get_string()));
        continue;
      }
      if (auto ok = add_pending_evidence(dve); !ok) {
        elog(fmt::format("failed to flush evidence from consensus_buffer to pending list: {}", ok.error()));
        continue;
      }
      ev_list.push_back(dve);
      ilog(fmt::format("verified new evidence of byzantine behavior: evidence={}", dve->get_string()));
    }
    // Reset consensus_buffer
    consensus_buffer.clear();
  }

  Result<std::shared_ptr<evidence>> bytes_to_ev(bytes ev_bytes) {
    ::tendermint::types::Evidence evpb;
    evpb.ParseFromArray(ev_bytes.data(), ev_bytes.size());
    return evidence::from_proto(evpb);
  }

  std::string ev_map_key(std::shared_ptr<evidence> ev) {
    return to_hex(ev->get_hash()); // TODO: check
  }

  bytes prefix_to_bytes(prefix);

  bytes key_committed(std::shared_ptr<evidence> ev);

  bytes key_pending(std::shared_ptr<evidence> ev);

  /// verify
  Result<void> verify(const std::shared_ptr<evidence>& ev);
  Result<void> verify_duplicate_vote(
    const duplicate_vote_evidence& ev, const std::string& chain_id, const std::shared_ptr<validator_set>& val_set);
  Result<void> verify_light_client_attack(const light_client_attack_evidence& ev,
    std::shared_ptr<signed_header> common_header,
    std::shared_ptr<signed_header> trusted_header,
    std::shared_ptr<validator_set> common_vals);
  Result<std::shared_ptr<signed_header>> get_signed_header(int64_t height);
};

} // namespace noir::consensus::ev
