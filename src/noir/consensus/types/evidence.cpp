// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/datastream.h>
#include <noir/common/types/varint.h>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/protobuf.h>

namespace noir::consensus {

std::shared_ptr<::tendermint::types::Evidence> evidence::evidence_to_proto(std::shared_ptr<evidence> ev) {
  if (!ev) {
    elog("evidence_to_proto failed: null evidence");
    return {};
  }
  auto ret = std::make_shared<::tendermint::types::Evidence>();
  if (auto ev_d = dynamic_cast<duplicate_vote_evidence*>(ev.get()); ev_d) {
    *ret->mutable_duplicate_vote_evidence() = *ev_d->to_proto();
    return ret;
  } else if (auto ev_l = dynamic_cast<light_client_attack_evidence*>(ev.get()); ev_l) {
    *ret->mutable_light_client_attack_evidence() = *ev_l->to_proto().value();
    return ret;
  }
  elog("evidence_to_proto failed: unknown evidence");
  return {};
}

std::shared_ptr<evidence> evidence::evidence_from_proto(std::shared_ptr<::tendermint::types::Evidence> ev) {}

std::vector<std::shared_ptr<::tendermint::abci::Evidence>> light_client_attack_evidence::get_abci() {
  std::vector<std::shared_ptr<::tendermint::abci::Evidence>> ret;
  for (auto& b_val : byzantine_validators) {
    auto ev = std::make_shared<::tendermint::abci::Evidence>();
    ev->set_type(::tendermint::abci::LIGHT_CLIENT_ATTACK);
    *ev->mutable_validator() = tm2pb::to_validator(b_val);
    ev->set_height(get_height());
    *ev->mutable_time() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(timestamp);
    ev->set_total_voting_power(total_voting_power);
    ret.emplace_back(ev);
  }
  return ret;
}

bytes light_client_attack_evidence::get_hash() {
  bytes buf(10);
  codec::basic_datastream<char> ds(buf);
  auto n = write_zigzag(ds, varint<int64_t>(common_height));
  bytes bz(32 + n);
  auto h = get_hash();
  std::copy(h.begin(), h.end(), bz.begin());
  std::copy(buf.begin(), buf.end(), bz.begin() + 32);
  return crypto::sha256()(bz);
}

std::vector<std::shared_ptr<validator>> light_client_attack_evidence::get_byzantine_validators(
  const std::shared_ptr<validator_set>& common_vals, const std::shared_ptr<signed_header>& trusted) {
  std::vector<std::shared_ptr<validator>> ret;
  if (conflicting_header_is_invalid(trusted->header)) {
    // a lunatic attack occurred; we should take validators in common_vals
    for (auto& commit_sig : conflicting_block->s_header->commit->signatures) {
      if (!commit_sig.for_block())
        continue;
      auto val = common_vals->get_by_address(commit_sig.validator_address);
      if (!val.has_value())
        continue;
      ret.emplace_back(std::make_shared<validator>(val.value()));
    }
    sort(ret.begin(), ret.end(), [](const std::shared_ptr<validator>& a, const std::shared_ptr<validator>& b) {
      return a->voting_power < b->voting_power;
    });
  } else if (trusted->commit->round == conflicting_block->s_header->commit->round) {
    // an equivocation attack occurred; we should find validators from conflicting light_block validator_set
    for (auto i = 0; i < conflicting_block->s_header->commit->signatures.size(); i++) {
      auto sig_A = conflicting_block->s_header->commit->signatures[i];
      if (!sig_A.for_block())
        continue;
      auto sig_B = trusted->commit->signatures[i];
      if (!sig_B.for_block())
        continue;
      auto val = conflicting_block->val_set->get_by_address(sig_A.validator_address);
      ret.emplace_back(std::make_shared<validator>(val.value()));
    }
  }
  return ret;
}

bool light_client_attack_evidence::conflicting_header_is_invalid(const std::shared_ptr<block_header>& trusted_header) {
  return trusted_header->validators_hash != conflicting_block->s_header->header->validators_hash ||
    trusted_header->next_validators_hash != conflicting_block->s_header->header->next_validators_hash ||
    trusted_header->consensus_hash != conflicting_block->s_header->header->consensus_hash ||
    trusted_header->app_hash != conflicting_block->s_header->header->app_hash ||
    trusted_header->last_results_hash != conflicting_block->s_header->header->last_results_hash;
}

} // namespace noir::consensus
