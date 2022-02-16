// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <noir/common/hex.h>
#include <noir/common/types.h>
#include <noir/consensus/bit_array.h>
#include <noir/consensus/tx.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

#include <fc/crypto/rand.hpp>
#include <utility>

namespace noir::consensus {

constexpr int64_t max_header_bytes{626};
constexpr int64_t max_overhead_for_block{11};

constexpr uint32_t block_part_size_bytes{65536};

inline int64_t max_commit_bytes(int val_count) {
  return 94 /* max_commit_overhead_bytes */ + (109 /* max_commit_sig_bytes */ * val_count);
}

enum block_id_flag {
  FlagAbsent = 1,
  FlagCommit,
  FlagNil
};

struct commit_sig {
  block_id_flag flag;
  bytes validator_address;
  p2p::tstamp timestamp;
  bytes signature;
  p2p::vote_extension_to_sign vote_extension;

  static commit_sig new_commit_sig_absent() {
    return commit_sig{FlagAbsent};
  }

  bool for_block() {
    return flag == FlagCommit;
  }

  bool absent() {
    return flag == FlagAbsent;
  }
};

struct commit {
  int64_t height;
  int32_t round;
  p2p::block_id my_block_id;
  std::vector<commit_sig> signatures;

  bytes hash; // NOTE: not to be serialized
  std::shared_ptr<bit_array> bit_array_{}; // NOTE: not to be serialized

  static commit new_commit(
    int64_t height_, int32_t round_, p2p::block_id block_id_, std::vector<commit_sig>& commit_sigs) {
    return commit{height_, round_, block_id_, commit_sigs};
  }

  size_t size() const {
    return signatures.size();
  }

  bytes get_hash() {
    if (hash.empty()) {
      for (auto sig : signatures) {
        // todo
      }
    }
    return hash;
  }

  std::shared_ptr<bit_array> get_bit_array() {
    if (bit_array_ == nullptr) {
      bit_array_ = bit_array::new_bit_array(signatures.size());
      for (auto i = 0; i < signatures.size(); i++)
        bit_array_->set_index(i, !signatures[i].absent());
    }
    return bit_array_;
  }

  template<typename T>
  inline friend T& operator<<(T& ds, const commit& v) {
    ds << v.height;
    ds << v.round;
    ds << v.my_block_id;
    ds << v.signatures;
    return ds;
  }

  template<typename T>
  inline friend T& operator>>(T& ds, commit& v) {
    ds >> v.height;
    ds >> v.round;
    ds >> v.my_block_id;
    ds >> v.signatures;
    return ds;
  }
};

struct part {
  uint32_t index;
  bytes bytes_;
  bytes proof;
};

struct part_set {
  uint32_t total{};
  bytes hash{};

  //  std::mutex mtx;
  std::vector<std::shared_ptr<part>> parts{};
  std::shared_ptr<bit_array> parts_bit_array{};
  uint32_t count{};
  int64_t byte_size{};

  part_set() = default;
  part_set(const part_set& p)
    : total(p.total), hash(p.hash), parts(p.parts), parts_bit_array(p.parts_bit_array), count(p.count),
      byte_size(p.byte_size) {}

  static std::shared_ptr<part_set> new_part_set_from_header(const p2p::part_set_header& header) {
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

  static std::shared_ptr<part_set> new_part_set_from_data(const bytes& data, uint32_t part_size) {
    // Divide data into 4KB parts
    uint32_t total = (data.size() + part_size - 1) / part_size;
    std::vector<std::shared_ptr<part>> parts(total);
    std::vector<bytes> parts_bytes(total);
    auto parts_bit_array = bit_array::new_bit_array(total);
    for (auto i = 0; i < total; i++) {
      auto first = data.begin() + (i * part_size);
      auto last = data.begin() + (std::min((uint32_t)data.size(), (i + 1) * part_size)) - 1;
      auto part_ = std::make_shared<part>();
      part_->index = i;
      std::copy(first, last, std::back_inserter(part_->bytes_));
      parts[i] = part_;
      parts_bytes[i] = part_->bytes_;
      parts_bit_array->set_index(i, true);
    }

    // Compute merkle proof - todo

    auto ret = std::make_shared<part_set>();
    ret->total = total;
    // ret->hash = root; // todo
    ret->parts = parts;
    ret->parts_bit_array = parts_bit_array;
    ret->count = total;
    ret->byte_size = data.size();
    return ret;
  }

  bool is_complete() {
    return count == total;
  }

  p2p::part_set_header header() {
    return p2p::part_set_header{total, hash};
  }

  bool has_header(p2p::part_set_header header_) {
    return header() == header_;
  }

  bool add_part(std::shared_ptr<part> part_) {
    // todo - lock mtx

    if (part_->index >= total) {
      dlog("error part set unexpected index");
      return false;
    }

    // If part already exists, return false.
    if (parts.size() > 0 && parts.size() >= part_->index) {
      if (parts[part_->index])
        return false;
    }

    // Check hash proof // todo

    // Add part
    parts[part_->index] = part_;
    parts_bit_array->set_index(part_->index, true);
    count++;
    byte_size += part_->bytes_.size();
    return true;
  }

  std::shared_ptr<part> get_part(int index) {
    // todo - lock mtx
    return parts[index]; // todo - check range?
  }

  std::shared_ptr<bit_array> get_bit_array() {
    // todo - lock mtx
    return parts_bit_array;
  }
};

struct block_data {
  std::vector<tx> txs;
  bytes hash;
};

struct block_header {
  std::string version;
  std::string chain_id;
  int64_t height{};
  p2p::tstamp time{};

  // Previous block info
  p2p::block_id last_block_id;

  // Hashes of block data
  bytes last_commit_hash;
  bytes data_hash;

  // Hashes from app for previous block
  bytes validators_hash;
  bytes next_validators_hash;
  bytes consensus_hash;
  bytes app_hash;
  bytes last_results_hash; // root hash of all results from txs of previous block

  // bytes evidence_hash;
  bytes proposer_address; // todo - use address type?

  bytes32 hash_; // todo - remove later after properly compute hash

  bytes get_hash() {
    // todo - properly compute hash
    if (hash_ == bytes32())
      fc::rand_pseudo_bytes(hash_.data(), hash_.data_size());
    return from_hex(hash_.str());
  }

  void populate(std::string& version_, std::string& chain_id_, p2p::tstamp timestamp_, p2p::block_id& last_block_id_,
    bytes val_hash, bytes next_val_hash, bytes consensus_hash_, bytes& app_hash_, bytes& last_results_hash_,
    bytes& proposer_address_) {
    version = version_;
    chain_id = chain_id_;
    time = timestamp_;
    last_block_id = last_block_id_;
    validators_hash = val_hash;
    next_validators_hash = next_val_hash;
    consensus_hash = consensus_hash_;
    app_hash = app_hash_;
    last_results_hash = last_results_hash_;
    proposer_address = proposer_address_;
  }
};

struct block {
  block_header header;
  block_data data;
  // evidence evidence;
  commit last_commit;

  std::mutex mtx;

  block() = default;
  block(block_header header_, block_data data_, commit last_commit_)
    : header(std::move(header_)), data(std::move(data_)), last_commit(std::move(last_commit_)) {}
  block(const block& b): header(b.header), data(b.data), last_commit(b.last_commit) {}

  void fill_header() {
    if (header.last_commit_hash.empty())
      header.last_commit_hash = last_commit.get_hash();
    if (header.data_hash.empty())
      header.data_hash = data.hash;
    // if (header.evidence_hash.empty())
    //   header.evidence_hash =
  }

  static block make_block(int64_t height, std::vector<tx>& txs, commit& last_commit /*, evidence */) {
    auto block_ = block{block_header{"", "", height}, block_data{txs}, last_commit};
    block_.fill_header();
    return block_;
  }

  /**
   * returns a part_set containing parts of a serialized block.
   * This is the form in which a block is gossipped to peers.
   */
  part_set make_part_set(uint32_t part_size);

  bytes get_hash() {
    std::lock_guard<std::mutex> g(mtx);
    // todo - implement
    return header.get_hash();
  }

  bool hashes_to(bytes& hash) {
    if (hash.empty())
      return false;
    return get_hash() == hash;
  }

  template<typename T>
  inline friend T& operator<<(T& ds, const block& v) {
    ds << v.header;
    ds << v.data;
    ds << v.last_commit;
    return ds;
  }

  template<typename T>
  inline friend T& operator>>(T& ds, block& v) {
    ds >> v.header;
    ds >> v.data;
    ds >> v.last_commit;
    return ds;
  }
};

using block_ptr = std::shared_ptr<block>;

} // namespace noir::consensus

NOIR_FOR_EACH_FIELD(noir::consensus::commit_sig, flag, validator_address, timestamp, signature, vote_extension)
NOIR_FOR_EACH_FIELD(noir::consensus::part, index, bytes_, proof)
// NOIR_FOR_EACH_FIELD(noir::consensus::part_set, total, hash, parts, parts_bit_array, count, byte_size)
NOIR_FOR_EACH_FIELD(noir::consensus::block_data, txs, hash)
NOIR_FOR_EACH_FIELD(noir::consensus::block_header, version, chain_id, height, time, last_block_id, last_commit_hash,
  consensus_hash, app_hash, last_results_hash, proposer_address, hash_)

template<>
struct noir::is_foreachable<noir::consensus::commit> : std::false_type {};

template<>
struct noir::is_foreachable<noir::consensus::block> : std::false_type {};
