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

result<void> evidence_pool::verify(std::shared_ptr<evidence> ev) {
  auto state_ = get_state();
  auto height = state_.last_block_height;
  auto evidence_params = state_.consensus_params_.evidence;
  auto age_num_blocks = height - ev->get_height();

  block_meta block_meta_;
  if (!block_store->load_block_meta(ev->get_height(), block_meta_))
    return make_unexpected(fmt::format("failed to verify evidence: missing block for height={}", ev->get_height()));

  auto ev_time = block_meta_.header.time;
  auto age_duration = state_.last_block_time - ev_time;

  if (age_duration > evidence_params.max_age_duration && age_num_blocks > evidence_params.max_age_num_blocks)
    return make_unexpected(fmt::format("evidence from height={} is too old", ev->get_height()));

  if (auto ev_d = dynamic_cast<duplicate_vote_evidence*>(ev.get()); ev_d) {
    auto val_set = std::make_shared<validator_set>();
    if (!state_db->load_validators(ev->get_height(), *val_set))
      return make_unexpected("evidence verify failed: unable to load validator");

    if (auto ok = verify_duplicate_vote(std::shared_ptr<duplicate_vote_evidence>(ev_d), state_.chain_id, val_set); !ok)
      return make_unexpected(ok.error());

    auto val_op = val_set->get_by_address(ev_d->vote_a->validator_address);
    auto val = std::make_shared<validator>(val_op.value());
    if (auto ok = ev_d->validate_abci(val, val_set, ev_time); !ok) {
      ev_d->generate_abci(val, val_set, ev_time);
      if (auto add_err = add_pending_evidence(ev); !add_err)
        elog(fmt::format("adding pending duplicate vote evidence failed: {}", add_err.error()));
      return make_unexpected(ok.error());
    }
    return {};

  } else if (auto ev_l = dynamic_cast<light_client_attack_evidence*>(ev.get()); ev_l) {
    auto common_header = get_signed_header(ev->get_height());
    if (!common_header)
      return make_unexpected(common_header.error());
    auto common_vals = std::make_shared<validator_set>();
    if (!state_db->load_validators(ev->get_height(), *common_vals))
      return make_unexpected("evidence verify failed: unable to load validator");
    auto trusted_header = common_header;

    if (ev->get_height() != ev_l->conflicting_block->s_header->header->height) {
      trusted_header = get_signed_header(ev_l->conflicting_block->s_header->header->height);
      if (!trusted_header) {
        auto latest_height = block_store->height();
        trusted_header = get_signed_header(latest_height);
        if (!trusted_header)
          return make_unexpected(trusted_header.error());
        if (trusted_header.value()->header->time < ev_l->conflicting_block->s_header->header->time)
          return make_unexpected(fmt::format("latest block time is before conflicting block time"));
      }
    }

    if (auto ok = verify_light_client_attack(std::shared_ptr<light_client_attack_evidence>(ev_l), common_header.value(),
          trusted_header.value(), common_vals);
        !ok)
      return make_unexpected(ok.error());

    if (auto ok = ev_l->validate_abci(common_vals, trusted_header.value(), ev_time); !ok) {
      ev_l->generate_abci(common_vals, trusted_header.value(), ev_time);
      if (auto add_err = add_pending_evidence(ev); !add_err)
        elog(fmt::format("adding pending duplicate vote evidence failed: {}", add_err.error()));
      return make_unexpected(ok.error());
    }
  }
  return make_unexpected("evidence verify failed: unknown evidence");
}

result<void> evidence_pool::verify_duplicate_vote(
  std::shared_ptr<duplicate_vote_evidence> ev, std::string chain_id, std::shared_ptr<validator_set> val_set) {
  auto val = val_set->get_by_address(ev->vote_a->validator_address);
  if (!val.has_value())
    return make_unexpected(fmt::format("address was not a validator at height={}", ev->get_height()));
  auto pub_key_ = val->pub_key_;

  if (ev->vote_a->height != ev->vote_b->height || ev->vote_a->round != ev->vote_b->round ||
    ev->vote_a->type != ev->vote_b->type)
    return make_unexpected(fmt::format("h/r/s do not match"));
  if (ev->vote_a->validator_address != ev->vote_b->validator_address)
    return make_unexpected(fmt::format("validator addresses do not match"));
  if (ev->vote_a->block_id_ != ev->vote_b->block_id_)
    return make_unexpected(fmt::format("block_ids do not match"));
  if (pub_key_.address() != ev->vote_a->validator_address)
    return make_unexpected(fmt::format("address does not match pub_key"));

  auto vote_a = ev->vote_a->to_proto();
  auto vote_b = ev->vote_b->to_proto();
  if (pub_key_.verify_signature(vote::vote_sign_bytes(chain_id, vote_a), ev->vote_a->signature))
    return make_unexpected(fmt::format("verifying vote_a: invalid signature"));
  if (pub_key_.verify_signature(vote::vote_sign_bytes(chain_id, vote_b), ev->vote_b->signature))
    return make_unexpected(fmt::format("verifying vote_b: invalid signature"));
  return {};
}

result<void> evidence_pool::verify_light_client_attack(std::shared_ptr<light_client_attack_evidence> ev,
  std::shared_ptr<signed_header> common_header,
  std::shared_ptr<signed_header> trusted_header,
  std::shared_ptr<validator_set> common_vals) {
  if (common_header->header->height != ev->conflicting_block->s_header->header->height) {
    auto commit_ = std::make_shared<commit>(ev->conflicting_block->s_header->commit.value());
    auto ok = common_vals->verify_commit_light_trusting(trusted_header->header->chain_id, commit_); // TODO: check
    if (!ok)
      return make_unexpected(fmt::format("skipping verification of conflicting block failed: {}", ok.error()));
  } else if (ev->conflicting_header_is_invalid(trusted_header->header)) {
    return make_unexpected(fmt::format("common height is the same as conflicting block height"));
  }

  if (auto ok = ev->conflicting_block->val_set->verify_commit_light(trusted_header->header->chain_id,
        ev->conflicting_block->s_header->commit->my_block_id, ev->conflicting_block->s_header->header->height,
        std::make_shared<commit>(ev->conflicting_block->s_header->commit.value()));
      !ok)
    return make_unexpected(fmt::format("invalid commit from conflicting block: {}", ok.error()));

  if (ev->conflicting_block->s_header->header->height > trusted_header->header->height &&
    ev->conflicting_block->s_header->header->time > trusted_header->header->time)
    return make_unexpected(fmt::format("conflicting block doesn't violate monotonically increasing time"));
  else if (ev->conflicting_block->s_header->header->get_hash() == trusted_header->header->get_hash())
    return make_unexpected(fmt::format("trusted header hash matches the evidence's conflicting header hash"));

  return {};
}

result<std::shared_ptr<signed_header>> evidence_pool::get_signed_header(int64_t height) {
  block_meta block_meta_;
  if (!block_store->load_block_meta(height, block_meta_))
    return make_unexpected(fmt::format("don't have a header at height={}", height));
  commit commit_;
  if (!block_store->load_block_commit(height, commit_))
    return make_unexpected(fmt::format("don't have a commit at height={}", height));
  auto ret = std::make_shared<signed_header>();
  ret->header = std::make_shared<noir::consensus::block_header>(block_meta_.header);
  ret->commit = commit_;
  return ret;
}
} // namespace noir::consensus::ev
