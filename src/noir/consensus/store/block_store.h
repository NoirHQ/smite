// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <noir/common/hex.h>
#include <noir/consensus/common.h>
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/block_meta.h>
#include <noir/consensus/types/light_block.h>
#include <noir/core/codec.h>
#include <noir/db/rocks_session.h>
#include <noir/db/session.h>

namespace noir::consensus {

/// \addtogroup consensus
/// \{

/// \brief BlockStore is a simple low level store for blocks.
/// There are three types of information stored:
///  - BlockMeta:   Meta information about each block
///  - Block part:  Parts of each block, aggregated w/ PartSet
///  - Commit:      The commit part of each block, for gossiping precommit votes
///
/// Currently the precommit signatures are duplicated in the Block parts as
/// well as the Commit.  In the future this may change, perhaps by moving
/// the Commit data outside the Block. (TODO)
///
/// The store can be assumed to contain all contiguous blocks between base and height (inclusive).
///
/// \note: BlockStore methods will panic if they encounter errors deserializing loaded data, indicating probable
/// corruption on disk.
class block_store {
  using db_session_type = noir::db::session::session<noir::db::session::rocksdb_t>;

public:
  explicit block_store(std::shared_ptr<db_session_type> session_): db_session_(std::move(session_)) {}

  block_store(block_store&& other) noexcept: db_session_(std::move(other.db_session_)) {}
  block_store(const block_store& other) noexcept: db_session_(other.db_session_) {}

  /// \brief gets the first known contiguous block height, or 0 for empty block stores.
  /// \return base height
  int64_t base() const {
    auto begin_it = db_session_->lower_bound_from_bytes(encode_key<prefix::block_meta>(1));
    auto end_it = db_session_->lower_bound_from_bytes(encode_key<prefix::block_meta>(0x1ll << 63));
    if (begin_it == end_it) {
      return 0;
    }
    auto tmp = begin_it.key();
    Bytes key_{tmp.begin(), tmp.end()};
    int64_t height_;
    check(decode_block_meta_key(key_, height_),
      fmt::format("unable to decode base key={}", to_string(key_))); // TODO: handle panic in consensus
    return height_;
  }

  /// \brief gets the last known contiguous block height, or 0 for empty block stores.
  /// \return height
  int64_t height() const {
    auto begin_it = db_session_->lower_bound_from_bytes(encode_key<prefix::block_meta>(0));
    auto end_it = db_session_->lower_bound_from_bytes(encode_key<prefix::block_meta>(0x1ll << 63));
    if (begin_it == end_it) { // no iterator found
      return 0;
    }
    auto tmp = (--end_it).key();
    Bytes key_{tmp.begin(), tmp.end()};
    int64_t height_;
    check(decode_block_meta_key(key_, height_),
      fmt::format("unable to decode height key={}", to_string(key_))); // TODO: handle panic in consensus
    return height_;
  }

  /// \brief gets the number of blocks in the block store.
  /// \return size
  int64_t size() const {
    int64_t height_ = height();
    return (height_ == 0) ? 0 : height_ + 1 - base();
  }

  /// \brief loads the base block meta
  /// \param[out] block_meta loaded block_meta object
  /// \return true on success, false otherwise
  bool load_base_meta(block_meta& bl_meta) const {
    auto tmp_it = db_session_->lower_bound_from_bytes(encode_key<prefix::block_meta>(1));
    auto tmp_bytes = tmp_it.key();
    Bytes key_{tmp_bytes.begin(), tmp_bytes.end()};
    int64_t height_;
    if (!decode_block_meta_key(key_, height_) || ((*tmp_it).second == std::nullopt)) {
      return false;
    }
    tmp_bytes = (*tmp_it).second.value();
    bl_meta = decode<block_meta>(Bytes{tmp_bytes.begin(), tmp_bytes.end()});
    return true;
  }

  /// \brief loads the block with the given height.
  /// \param[in] height_ height to load
  /// \param[out] bl loaded block object
  /// \return true on success, false otherwise
  bool load_block(int64_t height_, block& bl) const {
    block_meta bl_meta{};
    if (!load_block_meta(height_, bl_meta)) {
      return false;
    }
    auto parts_total = bl_meta.bl_id.parts.total;
    part part_{};
    Bytes data{};
    for (auto i = 0; i < parts_total; ++i) {
      // If the part is missing (e.g. since it has been deleted after we
      // loaded the block meta) we consider the whole block to be missing.
      if (!load_block_part(height_, i, part_)) {
        return false;
      }
      data.raw().insert(data.end(), part_.bytes_.begin(), part_.bytes_.end());
    }
    bl = decode<block>(data);
    return true;
  }

  /// \brief loads the block with the given hash.
  /// \param[in] hash hash to load
  /// \param[out] bl loaded block object
  /// \return true on success, false otherwise
  bool load_block_by_hash(const Bytes& hash, block& bl) const {
    auto tmp = db_session_->read_from_bytes(encode_key<prefix::block_hash>(hash.raw()));
    if (!tmp.has_value()) {
      return false;
    }
    auto height_ = decode_val(tmp.value());
    return load_block(height_, bl);
  }

  /// \brief loads the Part at the given index from the block at the given height.
  /// \param[in] height_ height to load
  /// \param[in] index index to load
  /// \param[out] part_ loaded part object
  /// \return true on success, false otherwise
  bool load_block_part(int64_t height_, int index, part& part_) const {
    auto tmp = db_session_->read_from_bytes(encode_key<prefix::block_part>(height_, index));
    if ((!tmp.has_value()) || tmp.value().empty()) {
      return false;
    }
    part_ = decode<part>(tmp.value());
    return true;
  }

  /// \brief loads the BlockMeta for the given height.
  /// \param[in] height_ height to load
  /// \param[out] block_meta_ loaded block_meta object
  /// \return true on success, false otherwise
  bool load_block_meta(int64_t height_, block_meta& block_meta_) const {
    auto tmp = db_session_->read_from_bytes(encode_key<prefix::block_meta>(height_));
    if ((!tmp.has_value()) || tmp.value().empty()) {
      return false;
    }
    block_meta_ = decode<block_meta>(tmp.value());
    return true;
  }

  /// \brief loads the Commit for the given height.
  /// \param[in] height_ height to load part
  /// \param[out] commit_ loaded commit object
  /// \return true on success, false otherwise
  bool load_block_commit(int64_t height_, commit& commit_) const {
    auto tmp = db_session_->read_from_bytes(encode_key<prefix::block_commit>(height_));
    if ((!tmp.has_value()) || tmp.value().empty()) {
      return false;
    }
    commit_ = decode<commit>(tmp.value());
    return true;
  }

  /// \brief loads the last locally seen Commit before being cannonicalized.
  /// This is useful when we've seen a commit,
  /// but there has not yet been a new block at `height + 1` that includes this commit in its block.last_commit.
  /// \param[out] commit_ loaded commit object
  /// \return true on success, false otherwise
  bool load_seen_commit(commit& commit_) const {
    auto tmp = db_session_->read_from_bytes(encode_key<prefix::seen_commit>());
    if ((!tmp.has_value()) || tmp.value().empty()) {
      return false;
    }
    commit_ = decode<commit>(tmp.value());
    return true;
  }

  /// \brief saves the given block, part_set, and seen_commit to the underlying db.
  /// part_set: Must be parts of the given block
  /// seen_commit: The +2/3 precommits that were seen which committed at height.
  ///             If all the nodes restart after committing a block,
  ///             we need this to reload the precommits to catch-up nodes to the
  ///             most recent height.  Otherwise they'd stall at H-1.
  /// \param[in] bl block object to save
  /// \param[in] bl_parts part_set object
  /// \param[in] seen_commit commit object
  /// \return true on success, false otherwise
  bool save_block(const block& bl, const part_set& bl_parts, const commit& seen_commit) {
    auto height_ = bl.header.height;
    auto hash_ = const_cast<block&>(bl).get_hash();
    auto parts_ = const_cast<part_set&>(bl_parts);
    batch_type batch{};

    // TODO: handle panic in consensus
    auto expected_height = height() + 1;
    check((base() == 0) || (height_ == expected_height),
      "BlockStore can only save contiguous blocks. Wanted %{}, got %{}", expected_height, height_);
    check(parts_.is_complete(), "BlockStore can only save complete block part sets");

    // Save block parts. This must be done before the block meta, since callers
    // typically load the block meta first as an indication that the block exists
    // and then go on to load block parts - we must make sure the block is
    // complete as soon as the block meta is written.
    for (auto i = 0; i < parts_.total; i++) {
      const auto part = parts_.get_part(i);
      save_block_part(height_, i, *part, batch);
    }

    {
      block_meta bl_meta = block_meta::new_block_meta(bl, bl_parts);
      auto buf = encode(bl_meta);
      batch.emplace_back(encode_key<prefix::block_meta>(height_), buf);
      batch.emplace_back(encode_key<prefix::block_hash>(hash_), encode_val(height_));
    }
    {
      auto buf = encode(*bl.last_commit); // TODO: handle case when last_commit is nullptr?
      batch.emplace_back(encode_key<prefix::block_commit>(height_ - 1), buf);
    }
    // Save seen commit (seen +2/3 precommits for block)
    {
      auto buf = encode(seen_commit);
      batch.emplace_back(encode_key<prefix::seen_commit>(), buf);
    }
    db_session_->write_from_bytes(batch);
    db_session_->commit();
    return true;
  }

  /// \brief saves a seen commit, used by e.g. the state sync reactor when bootstrapping node.
  /// \param[in] height_ height to save
  /// \param[in] seen_commit commit object to save
  /// \return true on success, false otherwise
  bool save_seen_commit(int64_t height_, const commit& seen_commit) {
    auto buf = encode(seen_commit);
    db_session_->write_from_bytes(encode_key<prefix::seen_commit>(), buf);
    db_session_->commit();
    return true;
  }

  /// \brief saves signed header
  /// \param[in] header signed_header object to save
  /// \param[in] block_id_ block_id object to save
  /// \return true on success, false otherwise
  bool save_signed_header(const signed_header& header, const noir::p2p::block_id& block_id_) {
    auto height_ = header.header->height;
    auto tmp = db_session_->find_from_bytes(encode_key<prefix::block_meta>(height_));
    if (tmp == db_session_->end()) {
      return false;
    }

    // FIXME: saving signed headers although necessary for proving evidence,
    // doesn't have complete parity with block meta's thus block size and num
    // txs are filled with negative numbers. We should aim to find a solution to
    // this.
    block_meta bm{
      .bl_id = block_id_,
      .bl_size = -1,
      .header = *header.header,
      .num_txs = -1,
    };

    auto buf = encode(bm);
    db_session_->write_from_bytes(encode_key<prefix::block_meta>(height_), buf);

    buf = encode(header.commit);
    db_session_->write_from_bytes(encode_key<prefix::block_commit>(height_), buf);
    db_session_->commit();
    return true;
  }

  /// \brief removes block up to (but not including) a given height.
  /// \param[in] height_ height
  /// \param[out] pruned the number of blocks pruned.
  /// \return true on success, false otherwise
  bool prune_blocks(int64_t height_, uint64_t& pruned) {
    uint64_t tmp = 0;
    pruned = 0;
    if (height_ <= 0) {
      // "height must be greater than 0"
      return false;
    }
    if (height_ > height()) {
      // "height must be equal to or less than the latest height %d", bs.Height()
      return false;
    }

    auto remove_block_hash = [&](const auto& k, const auto& v) {
      block_meta bm{};
      bm = decode<block_meta>(v);
      db_session_->erase_from_bytes(encode_key<prefix::block_meta>(bm.bl_id.hash));
      return true;
    };
    if (!prune_range(encode_key<prefix::block_meta>(0), encode_key<prefix::block_meta>(height_),
          std::optional<pre_deletion_hook_type>(remove_block_hash), pruned)) {
      return false;
    }
    if (!prune_range(
          encode_key<prefix::block_part>(0, 0), encode_key<prefix::block_part>(height_, 0), std::nullopt, tmp)) {
      return false;
    }
    // check(pruned == tmp);
    tmp = 0;
    if (!prune_range(
          encode_key<prefix::block_commit>(0), encode_key<prefix::block_commit>(height_), std::nullopt, tmp)) {
      return false;
    }
    // check(pruned == tmp);

    return true;
  }

private:
  using batch_type = std::vector<std::pair<Bytes, Bytes>>;
  enum class prefix : char {
    block_meta = 0,
    block_part = 1,
    block_commit = 2,
    seen_commit = 3,
    block_hash = 4,
  };

  std::shared_ptr<db_session_type> db_session_;

  static inline Bytes encode_val(int64_t val) {
    auto hex_ = hex::decode(fmt::format("{:016x}", static_cast<uint64_t>(val)));
    return {hex_.begin(), hex_.end()};
  }

  static inline int64_t decode_val(Bytes buf) {
    auto height_hex = hex::encode(buf);
    return stoull(height_hex, nullptr, 16);
  }

  template<typename... int64s>
  static Bytes encode_val(int64_t val, int64s... args) {
    auto hex_ = hex::decode(fmt::format("{:016x}", static_cast<uint64_t>(val)));
    Bytes lhs{hex_.begin(), hex_.end()};
    auto rhs = append_key(args...);
    lhs.raw().insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
  }

  template<prefix key_prefix>
  static Bytes encode_key() {
    Bytes key_{};
    key_.raw().push_back(static_cast<char>(key_prefix));
    return key_;
  }

  template<prefix key_prefix>
  static Bytes encode_key(int64_t val) {
    auto lhs = encode_key<key_prefix>();
    auto rhs = encode_val(val); // TODO: might need reserve() to optimize
    lhs.raw().insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
  }

  template<prefix key_prefix, typename... int64s>
  static Bytes encode_key(int64_t val, int64s... args) {
    auto lhs = encode_key<key_prefix>(val);
    auto rhs = encode_val(args...); // TODO: might need reserve() to optimize
    lhs.raw().insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
  }

  template<prefix key_prefix>
  static Bytes encode_key(Bytes bytes) {
    auto lhs = encode_key<key_prefix>();
    // std::string(bytes_.begin(), bytes_.end());
    lhs.raw().insert(lhs.end(), bytes.begin(), bytes.end());
    return lhs;
  }

  static bool decode_block_meta_key(const Bytes& key, int64_t& height_) {
    static constexpr size_t size_ = sizeof(char) + sizeof(int64_t);
    if ((key.size() != size_) || (key[0] != static_cast<char>(prefix::block_meta))) {
      return false;
    }

    height_ = decode_val(Bytes{key.begin() + 1, key.end()});
    return true;
  }

  bool save_block_part(int64_t height_, int index_, const part& part_, batch_type& batch) {
    auto buf = encode(part_);
    batch.emplace_back(encode_key<prefix::block_part>(height_, index_), buf);
    return true;
  }

  using pre_deletion_hook_type = std::function<bool(const Bytes& key, const Bytes& val)>;
  bool prune_range(const Bytes& start,
    const Bytes& end,
    std::optional<pre_deletion_hook_type> pre_deletion_hook,
    uint64_t& total_pruned) {
    Bytes start_ = start;

    // loop until we have finished iterating over all the keys by writing, opening a new batch
    // and incrementing through the next range of keys.
    while (start_ != end) {
      uint64_t pruned{0};
      if (!batch_delete(start_, end, pre_deletion_hook, pruned, start_)) {
        return false;
      }
      total_pruned += pruned;
      db_session_->commit();
    }

    return true;
  }

  bool batch_delete(const Bytes& start,
    const Bytes& end,
    std::optional<pre_deletion_hook_type> pre_deletion_hook,
    uint64_t& total_pruned,
    Bytes& new_end) {
    uint64_t pruned = 0;

    auto begin_ = db_session_->lower_bound_from_bytes(start);
    auto end_ = db_session_->lower_bound_from_bytes(end);

    for (auto it = begin_; it != end_; ++it) {
      auto key_ = it.key_from_bytes();
      if (pre_deletion_hook.has_value()) {
        auto val_ = it.value_from_bytes();
        if (val_->empty()) {
          return false;
        }
        if (!pre_deletion_hook.value()(key_, val_.value())) {
          return false;
        }
      }
      db_session_->erase_from_bytes(key_);

      if (++pruned >= 1000) {
        total_pruned = pruned;
        new_end = key_;
        return true;
      }
    }

    total_pruned = pruned;
    new_end = end;
    return true;
  }
};

/// }

} // namespace noir::consensus
