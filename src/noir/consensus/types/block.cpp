// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/protobuf.h>
#include <noir/common/log.h>
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/encoding_helper.h>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/vote.h>
#include <fmt/core.h>

namespace noir::consensus {

std::shared_ptr<vote> commit::get_vote(int32_t val_idx) {
  auto& commit_sig = signatures[val_idx];
  auto ret = std::make_shared<vote>();
  ret->type = noir::p2p::signed_msg_type::Precommit;
  ret->height = height;
  ret->round = round;
  ret->block_id_ = commit_sig.get_block_id(my_block_id);
  ret->timestamp = commit_sig.timestamp;
  ret->validator_address = commit_sig.validator_address;
  ret->validator_index = val_idx;
  ret->signature = commit_sig.signature;

  return std::move(ret);
}

std::shared_ptr<vote_set> commit_to_vote_set(
  const std::string& chain_id, commit& commit_, const std::shared_ptr<validator_set>& val_set) {
  auto vote_set_ =
    vote_set::new_vote_set(chain_id, commit_.height, commit_.round, p2p::signed_msg_type::Precommit, val_set);
  check(vote_set_ != nullptr);
  for (auto it = commit_.signatures.begin(); it != commit_.signatures.end(); ++it) {
    auto index = std::distance(commit_.signatures.begin(), it);
    if (it->absent()) {
      continue; // OK, some precommits can be missing.
    }

    auto vote_ = commit_.get_vote(static_cast<int32_t>(index));
    auto [_, err] = vote_set_->add_vote(vote_);
    check(!err, fmt::format("Failed to reconstruct LastCommit {}", err.message()));
  }
  return std::move(vote_set_);
}

Bytes commit::get_hash() {
  if (hash.empty()) {
    merkle::bytes_list items;
    for (const auto& sig : signatures) {
      auto pb = commit_sig::to_proto(sig);
      auto bz = codec::protobuf::encode(*pb);
      items.push_back(bz);
    }
    hash = merkle::hash_from_bytes_list(items);
  }
  return hash;
}

std::shared_ptr<part_set> part_set::new_part_set_from_header(const p2p::part_set_header& header) {
  std::vector<std::shared_ptr<part>> parts_;
  parts_.resize(header.total);
  auto ret = std::make_shared<part_set>();
  ret->total = header.total;
  ret->hash = header.hash;
  ret->parts = parts_;
  ret->parts_bit_array = bit_array::new_bit_array(header.total);
  ret->count = 0;
  ret->byte_size = 0;
  return ret;
}

std::shared_ptr<part_set> part_set::new_part_set_from_data(const Bytes& data, uint32_t part_size) {
  // Divide data into 4KB parts
  uint32_t total = (data.size() + part_size - 1) / part_size;
  std::vector<std::shared_ptr<part>> parts(total);
  std::vector<Bytes> parts_bytes(total);
  auto parts_bit_array = bit_array::new_bit_array(total);
  for (auto i = 0; i < total; i++) {
    auto first = data.begin() + (i * part_size);
    auto last = data.begin() + (std::min((uint32_t)data.size(), (i + 1) * part_size));
    auto part_ = std::make_shared<part>();
    part_->index = i;
    std::copy(first, last, std::back_inserter(part_->bytes_.raw()));
    parts[i] = part_;
    parts_bytes[i] = part_->bytes_;
    parts_bit_array->set_index(i, true);
  }

  // Compute merkle proof
  auto [root, proofs] = merkle::proofs_from_bytes_list(parts_bytes);
  for (auto i = 0; i < total; i++) {
    parts[i]->proof_ = *(proofs[i]);
  }

  auto ret = std::make_shared<part_set>();
  ret->total = total;
  ret->hash = root;
  ret->parts = parts;
  ret->parts_bit_array = parts_bit_array;
  ret->count = total;
  ret->byte_size = data.size();
  return ret;
}

bool part_set::add_part(std::shared_ptr<part> part_) {
  std::scoped_lock g(mtx);

  if (part_->index >= total) {
    elog("error part set unexpected index");
    return false;
  }

  // If part already exists, return false.
  if (parts.size() > 0 && parts.size() >= part_->index) {
    if (parts[part_->index])
      return false;
  }

  // Check hash proof
  if (auto err = part_->proof_.verify(get_hash(), part_->bytes_); err.has_value()) {
    elog("error part set invalid proof");
    return false;
  }

  // Add part
  parts[part_->index] = part_;
  parts_bit_array->set_index(part_->index, true);
  count++;
  byte_size += part_->bytes_.size();
  return true;
}

Bytes part_set::get_hash() {
  return hash;
}

Bytes block_data::get_hash() {
  if (hash.empty()) {
    merkle::bytes_list items;
    for (const auto& tx : txs)
      items.push_back(crypto::Sha256()(tx));
    hash = merkle::hash_from_bytes_list(items);
  }
  return hash;
}

Bytes block_header::get_hash() {
  if (validators_hash.empty())
    return {};

  auto pb_v = consensus_version::to_proto(version);
  auto bz_v = codec::protobuf::encode(*pb_v);

  auto pb_lbi = p2p::block_id::to_proto(last_block_id);
  auto bz_lbi = codec::protobuf::encode(*pb_lbi);

  merkle::bytes_list items;
  items.push_back(bz_v);
  items.push_back(cdc_encode(chain_id));
  items.push_back(cdc_encode(height));
  items.push_back(cdc_encode_time(time));
  items.push_back(bz_lbi);
  items.push_back(cdc_encode(last_commit_hash));
  items.push_back(cdc_encode(data_hash));
  items.push_back(cdc_encode(validators_hash));
  items.push_back(cdc_encode(next_validators_hash));
  items.push_back(cdc_encode(consensus_hash));
  items.push_back(cdc_encode(app_hash));
  items.push_back(cdc_encode(last_results_hash));
  items.push_back(cdc_encode(evidence_hash));
  items.push_back(cdc_encode(proposer_address));
  return merkle::hash_from_bytes_list(items);
}

Bytes evidence_data::get_hash() {
  if (hash.empty()) {
    if (!evs)
      evs = std::make_shared<evidence_list>();
    hash = evs->hash();
  }
  return hash;
}

Result<std::unique_ptr<::tendermint::types::EvidenceList>> evidence_data::to_proto(const evidence_data& e) {
  if (!e.evs)
    return std::make_unique<::tendermint::types::EvidenceList>();
  return evidence_list::to_proto(*e.evs);
}

Result<std::shared_ptr<evidence_data>> evidence_data::from_proto(const ::tendermint::types::EvidenceList& pb) {
  auto ret = std::make_shared<evidence_data>();
  ret->evs = std::make_shared<evidence_list>();
  for (auto& i : pb.evidence()) {
    auto ev = evidence::from_proto(i);
    if (!ev)
      return ev.error();
    ret->evs->list.push_back(ev.value());
  }
  ret->byte_size = pb.ByteSizeLong();
  return ret;
}

std::shared_ptr<block> block::new_block_from_part_set(const std::shared_ptr<part_set>& ps) {
  if (!ps->is_complete())
    return {};
  Bytes bz;
  bz.raw().reserve(ps->byte_size);
  for (const auto& p : ps->parts)
    std::copy(p->bytes_.begin(), p->bytes_.end(), std::back_inserter(bz.raw()));
  return block::from_proto(codec::protobuf::decode<::tendermint::types::Block>(bz));
}

std::shared_ptr<part_set> block::make_part_set(uint32_t part_size) {
  std::scoped_lock g(mtx);
  auto bz = codec::protobuf::encode(*block::to_proto(*this));
  return part_set::new_part_set_from_data(bz, part_size);
}

std::unique_ptr<::tendermint::types::Block> block::to_proto(const block& b) {
  auto ret = std::make_unique<::tendermint::types::Block>();
  ret->set_allocated_header(block_header::to_proto(b.header).release());
  ret->set_allocated_data(block_data::to_proto(b.data).release());
  if (b.last_commit)
    ret->set_allocated_last_commit(commit::to_proto(*b.last_commit).release());
  auto ev = evidence_data::to_proto(b.evidence); // TODO : handle error
  ret->set_allocated_evidence(ev.value().release());
  return ret;
}

std::shared_ptr<block> block::from_proto(const ::tendermint::types::Block& pb) {
  auto ret = std::make_shared<block>();
  ret->header = *block_header::from_proto(pb.header());
  ret->data = *block_data::from_proto(pb.data());
  if (pb.has_last_commit())
    ret->last_commit = commit::from_proto(pb.last_commit());
  ret->evidence = *evidence_data::from_proto(pb.evidence()).value(); // TODO : handle error
  ret->validate_basic(); // TODO
  return ret;
}

} // namespace noir::consensus
