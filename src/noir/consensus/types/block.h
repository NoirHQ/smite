// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/for_each.h>
#include <noir/common/hex.h>
#include <noir/common/types.h>
#include <noir/consensus/bit_array.h>
#include <noir/consensus/merkle/proof.h>
#include <noir/consensus/tx.h>
#include <noir/consensus/version.h>
#include <noir/crypto/rand.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>
#include <tendermint/types/block.pb.h>

#include <fmt/core.h>

#include <memory>
#include <utility>

namespace noir::consensus {

constexpr int64_t max_header_bytes{626};
constexpr int64_t max_overhead_for_block{11};

constexpr uint32_t block_part_size_bytes{65536};

inline int64_t max_commit_bytes(int val_count) {
  return 94 /* max_commit_overhead_bytes */ + (109 /* max_commit_sig_bytes */ * val_count);
}

inline int64_t max_data_bytes(int64_t max_bytes, int64_t evidence_bytes, int vals_count) {
  auto max_data_bytes =
    max_bytes - max_overhead_for_block - max_header_bytes - max_commit_bytes(vals_count) - evidence_bytes;
  check(max_bytes >= 0, "negative max_data_bytes");
  return max_data_bytes;
}

inline Bytes random_address() {
  Bytes ret{20};
  noir::crypto::rand_bytes(ret);
  return ret;
}
inline Bytes random_hash() {
  Bytes ret{32};
  noir::crypto::rand_bytes(ret);
  return ret;
}

enum block_id_flag {
  FlagAbsent = 1,
  FlagCommit,
  FlagNil
};

struct commit_sig {
  block_id_flag flag;
  Bytes validator_address;
  tstamp timestamp;
  Bytes signature;

  static commit_sig new_commit_sig_absent() {
    return commit_sig{FlagAbsent};
  }

  bool for_block() {
    return flag == FlagCommit;
  }

  bool absent() {
    return flag == FlagAbsent;
  }

  p2p::block_id get_block_id(p2p::block_id commit_block_id) {
    switch (flag) {
    case block_id_flag::FlagCommit: {
      return commit_block_id;
    }
    case block_id_flag::FlagAbsent:
    case block_id_flag::FlagNil: {
      return p2p::block_id{};
    }
    default: {
      check(false, fmt::format("Unknown BlockIDFlag: {}", flag));
      __builtin_unreachable();
    }
    }
  }

  static std::unique_ptr<::tendermint::types::CommitSig> to_proto(const commit_sig& c) {
    auto ret = std::make_unique<::tendermint::types::CommitSig>();
    ret->set_validator_address({c.validator_address.begin(), c.validator_address.end()});
    *ret->mutable_timestamp() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(c.timestamp); // TODO
    ret->set_signature({c.signature.begin(), c.signature.end()});
    return ret;
  }

  static std::shared_ptr<commit_sig> from_proto(const ::tendermint::types::CommitSig& pb) {
    auto ret = std::make_shared<commit_sig>();
    ret->flag = (block_id_flag)pb.block_id_flag();
    ret->validator_address = {pb.validator_address().begin(), pb.validator_address().end()};
    ret->timestamp = ::google::protobuf::util::TimeUtil::TimestampToMicroseconds(pb.timestamp());
    ret->signature = {pb.signature().begin(), pb.signature().end()};
    return ret;
  }
};

struct commit {
  int64_t height{};
  int32_t round{};
  p2p::block_id my_block_id{};
  std::vector<commit_sig> signatures{};

  Bytes hash; // NOTE: not to be serialized
  std::shared_ptr<bit_array> bit_array_{}; // NOTE: not to be serialized

  [[nodiscard]] static std::shared_ptr<commit> new_commit(
    int64_t height_, int32_t round_, const p2p::block_id& block_id_, const std::vector<commit_sig>& commit_sigs) {
    return std::make_shared<commit>(
      commit{.height = height_, .round = round_, .my_block_id = block_id_, .signatures = commit_sigs});
  }

  size_t size() const {
    return signatures.size();
  }

  Bytes get_hash();

  /// \brief GetVote converts the CommitSig for the given valIdx to a Vote. Returns nil if the precommit at valIdx is
  /// nil. Panics if valIdx >= commit.Size().
  /// \param[in] val_idx
  /// \return shared_ptr of vote
  std::shared_ptr<struct vote> get_vote(int32_t val_idx);

  std::shared_ptr<bit_array> get_bit_array() {
    if (bit_array_ == nullptr) {
      bit_array_ = bit_array::new_bit_array(signatures.size());
      for (auto i = 0; i < signatures.size(); i++)
        bit_array_->set_index(i, !signatures[i].absent());
    }
    return bit_array_;
  }

  static std::unique_ptr<::tendermint::types::Commit> to_proto(const commit& c) {
    auto ret = std::make_unique<::tendermint::types::Commit>();
    auto sigs = ret->mutable_signatures();
    for (auto& sig : c.signatures)
      sigs->AddAllocated(commit_sig::to_proto(sig).release());
    ret->set_height(c.height);
    ret->set_round(c.round);
    ret->set_allocated_block_id(p2p::block_id::to_proto(c.my_block_id).release());
    return ret;
  }

  static std::unique_ptr<commit> from_proto(const ::tendermint::types::Commit& pb) {
    auto ret = std::make_unique<commit>();

    auto bi = p2p::block_id::from_proto(pb.block_id());
    ret->my_block_id = *bi;

    auto pb_sigs = pb.signatures();
    for (const auto& sig : pb_sigs)
      ret->signatures.push_back(*commit_sig::from_proto(sig));

    ret->height = pb.height();
    ret->round = pb.round();

    // ret->validate_basic(); // TODO

    return ret;
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
  Bytes bytes_;
  merkle::proof proof_;
};

struct part_set {
  uint32_t total{};
  Bytes hash{};

  std::vector<std::shared_ptr<part>> parts{};
  std::shared_ptr<bit_array> parts_bit_array{};
  uint32_t count{};
  int64_t byte_size{};
  std::mutex mtx;

  part_set() = default;
  part_set(const part_set& p)
    : total(p.total),
      hash(p.hash),
      parts(p.parts),
      parts_bit_array(p.parts_bit_array),
      count(p.count),
      byte_size(p.byte_size) {}

  static std::shared_ptr<part_set> new_part_set_from_header(const p2p::part_set_header& header);

  static std::shared_ptr<part_set> new_part_set_from_data(const Bytes& data, uint32_t part_size);

  bool add_part(std::shared_ptr<part> part_);

  bool is_complete() {
    return count == total;
  }

  p2p::part_set_header header() {
    return p2p::part_set_header{total, hash};
  }

  bool has_header(p2p::part_set_header header_) {
    if (this == nullptr) ///< NOT a very nice way of coding; need to refactor later
      return false;
    return header() == header_;
  }

  std::shared_ptr<part> get_part(int index) {
    std::scoped_lock g(mtx);
    return parts[index]; // todo - check range?
  }

  std::shared_ptr<bit_array> get_bit_array() {
    std::scoped_lock g(mtx);
    return parts_bit_array;
  }

  Bytes get_hash();
};

struct block_data {
  std::vector<tx> txs;
  Bytes hash; // may continuously change

  Bytes get_hash();

  static std::unique_ptr<::tendermint::types::Data> to_proto(const block_data& b) {
    auto ret = std::make_unique<::tendermint::types::Data>();
    auto pb_txs = ret->mutable_txs();
    for (const auto& tx : b.txs)
      pb_txs->Add({tx.begin(), tx.end()});
    return ret;
  }

  static std::shared_ptr<block_data> from_proto(const ::tendermint::types::Data& pb) {
    auto ret = std::make_shared<block_data>();
    auto pb_txs = pb.txs();
    for (const auto& tx : pb_txs)
      ret->txs.push_back({tx.begin(), tx.end()});
    return ret;
  }
};

struct block_header {
  consensus_version version;
  std::string chain_id;
  int64_t height{};
  tstamp time{};

  // Previous block info
  p2p::block_id last_block_id;

  // Hashes of block data
  Bytes last_commit_hash;
  Bytes data_hash;

  // Hashes from app for previous block
  Bytes validators_hash;
  Bytes next_validators_hash;
  Bytes consensus_hash;
  Bytes app_hash;
  Bytes last_results_hash; // root hash of all results from txs of previous block

  Bytes evidence_hash;
  Bytes proposer_address; // todo - use address type?

  Bytes get_hash();

  void populate(consensus::consensus_version& version_,
    std::string& chain_id_,
    tstamp timestamp_,
    p2p::block_id& last_block_id_,
    Bytes val_hash,
    Bytes next_val_hash,
    Bytes consensus_hash_,
    Bytes& app_hash_,
    Bytes& last_results_hash_,
    Bytes& proposer_address_) {
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

  std::optional<std::string> validate_basic() {
    if (chain_id.length() > 50)
      return "chain_id is too long";
    if (height < 0)
      return "negative height";
    else if (height == 0)
      return "zero height";
    // last_block_id->validate_basic();
    // validate_hash(last_commit_hash);
    // validate_hash(data_hash);
    // validate_hash(evidence_hash);
    // todo - add more
    return {};
  }

  static Result<block_header> make_header(block_header&& h) {
    if (h.version.block == 0)
      h.version.block = block_protocol;
    if (h.height == 0)
      h.height = 1;
    if (h.last_block_id.is_zero())
      h.last_block_id = {.hash = random_hash(), .parts = {.total = 100, .hash = random_hash()}};
    if (h.chain_id.empty())
      h.chain_id = "test-chain";
    if (h.last_commit_hash.empty())
      h.last_commit_hash = random_hash();
    if (h.data_hash.empty())
      h.data_hash = random_hash();
    if (h.validators_hash.empty())
      h.validators_hash = random_hash();
    if (h.next_validators_hash.empty())
      h.next_validators_hash = random_hash();
    if (h.consensus_hash.empty())
      h.consensus_hash = random_hash();
    if (h.app_hash.empty())
      h.app_hash = random_hash();
    if (h.last_results_hash.empty())
      h.last_results_hash = random_hash();
    if (h.evidence_hash.empty())
      h.evidence_hash = random_hash();
    if (h.proposer_address.empty())
      h.proposer_address = random_address();
    if (auto r = h.validate_basic(); r.has_value())
      return Error::format("{}", r.value());
    return h;
  }

  static std::unique_ptr<::tendermint::types::Header> to_proto(const block_header& b) {
    auto ret = std::make_unique<::tendermint::types::Header>();
    *ret->mutable_version(); // TODO
    *ret->mutable_chain_id() = b.chain_id;
    ret->set_height(b.height);
    *ret->mutable_time() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(b.time); // TODO
    ret->set_allocated_last_block_id(p2p::block_id::to_proto(b.last_block_id).release());
    ret->set_validators_hash({b.validators_hash.begin(), b.validators_hash.end()});
    ret->set_next_validators_hash({b.next_validators_hash.begin(), b.next_validators_hash.end()});
    ret->set_consensus_hash({b.consensus_hash.begin(), b.consensus_hash.end()});
    ret->set_app_hash({b.app_hash.begin(), b.app_hash.end()});
    ret->set_data_hash({b.data_hash.begin(), b.data_hash.end()});
    ret->set_evidence_hash({b.evidence_hash.begin(), b.evidence_hash.end()});
    ret->set_last_results_hash({b.last_results_hash.begin(), b.last_results_hash.end()});
    ret->set_last_commit_hash({b.last_commit_hash.begin(), b.last_commit_hash.end()});
    ret->set_proposer_address({b.proposer_address.begin(), b.proposer_address.end()});
    return ret;
  }

  static std::shared_ptr<block_header> from_proto(const ::tendermint::types::Header& pb) {
    auto ret = std::make_shared<block_header>();

    auto bi = p2p::block_id::from_proto(pb.last_block_id()); // TODO: handle error case

    auto version_pb = pb.version();
    ret->version = {.block = version_pb.block(), .app = version_pb.app()};
    ret->chain_id = pb.chain_id();
    ret->height = pb.height();
    ret->time = ::google::protobuf::util::TimeUtil::TimestampToMicroseconds(pb.time());
    ret->last_block_id = *bi;
    ret->validators_hash = {pb.validators_hash().begin(), pb.validators_hash().end()};
    ret->next_validators_hash = {pb.next_validators_hash().begin(), pb.next_validators_hash().end()};
    ret->consensus_hash = {pb.consensus_hash().begin(), pb.consensus_hash().end()};
    ret->app_hash = {pb.app_hash().begin(), pb.app_hash().end()};
    ret->data_hash = {pb.data_hash().begin(), pb.data_hash().end()};
    ret->evidence_hash = {pb.evidence_hash().begin(), pb.evidence_hash().end()};
    ret->last_results_hash = {pb.last_results_hash().begin(), pb.last_results_hash().end()};
    ret->last_commit_hash = {pb.last_commit_hash().begin(), pb.last_commit_hash().end()};
    ret->proposer_address = {pb.proposer_address().begin(), pb.proposer_address().end()};

    ret->validate_basic(); // TODO

    return ret;
  }
};

struct block {
  block_header header;
  block_data data;
  // evidence evidence;
  std::unique_ptr<commit> last_commit;

  std::mutex mtx;

  block() {}
  ~block() {}
  block(block_header&& header_, block_data&& data_, std::unique_ptr<commit>&& last_commit_)
    : header(std::move(header_)), data(std::move(data_)), last_commit(std::move(last_commit_)) {}
  block(const block& b)
    : header(b.header), data(b.data), last_commit(b.last_commit ? std::make_unique<commit>(*b.last_commit) : nullptr) {}
  auto& operator=(const block& b) {
    std::scoped_lock g(mtx);
    header = b.header;
    data = b.data;
    // evidence = b.evidence;
    last_commit = b.last_commit ? std::make_unique<commit>(*b.last_commit) : nullptr;
    return *this;
  }
  block(block&& b) noexcept
    : header(std::move(b.header)), data(std::move(b.data)), last_commit(std::move(b.last_commit)) {}
  auto& operator=(block&& b) noexcept {
    std::scoped_lock g(mtx);
    header = std::move(b.header);
    data = std::move(b.data);
    // evidence = b.evidence;
    last_commit = std::move(b.last_commit);
    return *this;
  }

  static std::shared_ptr<block> new_block_from_part_set(const std::shared_ptr<part_set>& ps);

  std::optional<std::string> validate_basic() {
    std::scoped_lock g(mtx);

    if (auto err = header.validate_basic(); err.has_value())
      return "invalid header: " + err.value();

    // Validate last commit
    // if (last_commit.height == 0)
    //   return "unknown last_commit";

    // if (last_commit.get_hash() != header.last_commit_hash)
    //  return "wrong last_commit_hash";

    return {};
  }

  void fill_header() {
    if (header.last_commit_hash.empty() && last_commit)
      header.last_commit_hash = last_commit->get_hash();
    if (header.data_hash.empty())
      header.data_hash = data.get_hash();
    // if (header.evidence_hash.empty())
    //   header.evidence_hash =
  }

  static std::shared_ptr<block> make_block(
    int64_t height, const std::vector<tx>& txs, const std::shared_ptr<commit>& last_commit /*, evidence */) {
    auto block_ = std::make_shared<block>(block{block_header{{.block = block_protocol, .app = 0}, "", height},
      block_data{txs}, std::make_unique<commit>(*last_commit)});
    block_->fill_header();
    return block_;
  }

  /**
   * returns a part_set containing parts of a serialized block.
   * This is the form in which a block is gossipped to peers.
   */
  std::shared_ptr<part_set> make_part_set(uint32_t part_size);

  Bytes get_hash() {
    if (this == nullptr)
      return {};
    std::scoped_lock g(mtx);
    if (!last_commit)
      return {};
    fill_header();
    return header.get_hash();
  }

  bool hashes_to(Bytes& hash) {
    if (hash.empty())
      return false;
    return get_hash() == hash;
  }

  static std::unique_ptr<::tendermint::types::Block> to_proto(const block& b) {
    auto ret = std::make_unique<::tendermint::types::Block>();
    ret->set_allocated_header(block_header::to_proto(b.header).release());
    ret->set_allocated_data(block_data::to_proto(b.data).release());
    if (b.last_commit)
      ret->set_allocated_last_commit(commit::to_proto(*b.last_commit).release());

    // TODO: add evidence

    return ret;
  }

  static std::shared_ptr<block> from_proto(const ::tendermint::types::Block& pb) {
    auto ret = std::make_shared<block>();
    ret->header = *block_header::from_proto(pb.header());
    ret->data = *block_data::from_proto(pb.data());
    if (pb.has_last_commit())
      ret->last_commit = commit::from_proto(pb.last_commit());
    ret->validate_basic(); // TODO
    return ret;
  }

  template<typename T>
  inline friend T& operator<<(T& ds, const block& v) {
    ds << v.header;
    ds << v.data;
    ds << bool(!!v.last_commit);
    if (!!v.last_commit)
      ds << *v.last_commit;
    return ds;
  }

  template<typename T>
  inline friend T& operator>>(T& ds, block& v) {
    ds >> v.header;
    ds >> v.data;
    bool b;
    ds >> b;
    if (b) {
      v.last_commit = std::make_unique<commit>();
      ds >> *v.last_commit;
    }
    return ds;
  }
};

/// \brief  CommitToVoteSet constructs a VoteSet from the Commit and validator set. Panics if signatures from the commit
/// can't be added to the voteset. Inverse of VoteSet.MakeCommit().
/// \param[in] chain_id
/// \param[in] commit_
/// \param[in] val_set
/// \return shared_ptr of vote_set
std::shared_ptr<struct vote_set> commit_to_vote_set(
  const std::string& chain_id, commit& commit_, const std::shared_ptr<struct validator_set>& val_set);

using block_ptr = std::shared_ptr<block>;

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::commit_sig, flag, validator_address, timestamp, signature);
NOIR_REFLECT(noir::consensus::part, index, bytes_, proof_);
// NOIR_REFLECT(noir::consensus::part_set, total, hash, parts, parts_bit_array, count, byte_size);
NOIR_REFLECT(noir::consensus::block_data, txs, hash);
NOIR_REFLECT(noir::consensus::block_header, version, chain_id, height, time, last_block_id, last_commit_hash, data_hash,
  validators_hash, next_validators_hash, consensus_hash, app_hash, last_results_hash, proposer_address);

template<>
struct noir::IsForeachable<noir::consensus::commit> : std::false_type {};

template<>
struct noir::IsForeachable<noir::consensus::block> : std::false_type {};
