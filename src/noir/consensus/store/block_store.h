// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/consensus/block.h>
#include <noir/consensus/types.h>
#include <noir/db/rocks_session.hpp>
#include <noir/db/session.hpp>

namespace noir::consensus {

// TODO: move to block_meta.h
struct block_meta {
  noir::p2p::block_id bl_id;
  int32_t bl_size;
  noir::consensus::block_header header;
  int32_t num_txs;
  static block_meta new_block_meta(const block& bl_, const part_set& bl_parts) {
    return {};
  }
};

// TODO: move to light_block.h?
struct signed_header {
  noir::consensus::block_header header;
  std::optional<noir::consensus::commit> commit;
};

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
  explicit block_store(std::shared_ptr<db_session_type> session_) : db_session_(std::move(session_)) {}

  block_store(block_store&& other) noexcept : db_session_(std::move(other.db_session_)) {}
  block_store(const block_store& other) noexcept : db_session_(other.db_session_) {}

  /// \brief gets the first known contiguous block height, or 0 for empty block stores.
  /// \return base height
  int64_t base() const {
    return 0;
  }

  /// \brief gets the last known contiguous block height, or 0 for empty block stores.
  /// \return height
  int64_t height() const {
    return 0;
  }

  /// \brief gets the number of blocks in the block store.
  /// \return size
  int64_t size() const {
    return 0;
  }

  /// \brief loads the base block meta
  /// \param[out] block_meta loaded block_meta object
  /// \return true on success, false otherwise
  bool load_base_meta(block_meta& bl_meta) const {
    return true;
  }

  /// \brief loads the block with the given height.
  /// \param[in] height_ height to load
  /// \param[out] bl loaded block object
  /// \return true on success, false otherwise
  bool load_block(int64_t height_, block& bl) const {
    return true;
  }

  /// \brief loads the block with the given hash.
  /// \param[in] hash hash to load
  /// \param[out] bl loaded block object
  /// \return true on success, false otherwise
  bool load_block_by_hash(const noir::p2p::bytes& hash, block& bl) const {
    return true;
  }

  /// \brief loads the Part at the given index from the block at the given height.
  /// \param[in] height_ height to load
  /// \param[in] index index to load
  /// \param[out] part_ loaded part object
  /// \return true on success, false otherwise
  bool load_block_part(int64_t height_, int index, part& part_) const {
    return true;
  }

  /// \brief loads the BlockMeta for the given height.
  /// \param[in] height_ height to load
  /// \param[out] block_meta_ loaded block_meta object
  /// \return true on success, false otherwise
  bool load_block_meta(int64_t height_, block_meta& block_meta_) const {
    return true;
  }

  /// \brief loads the Commit for the given height.
  /// \param[in] height_ height to load part
  /// \param[out] commit_ loaded commit object
  /// \return true on success, false otherwise
  bool load_block_commit(int64_t height_, commit& commit_) const {
    return true;
  }

  /// \brief loads the last locally seen Commit before being cannonicalized.
  /// This is useful when we've seen a commit,
  /// but there has not yet been a new block at `height + 1` that includes this commit in its block.last_commit.
  /// \param[out] commit_ loaded commit object
  /// \return true on success, false otherwise
  bool load_seen_commit(commit& commit_) const {
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
    return true;
  }

  /// \brief saves a seen commit, used by e.g. the state sync reactor when bootstrapping node.
  /// \param[in] height_ height to save
  /// \param[in] seen_commit commit object to save
  /// \return true on success, false otherwise
  bool save_seen_commit(int64_t height_, const commit& seen_commit) {
    return true;
  }

  /// \brief saves signed header
  /// \param[in] header signed_header object to save
  /// \param[in] block_id_ block_id object to save
  /// \return true on success, false otherwise
  bool save_signed_header(const signed_header& header, const noir::p2p::block_id& block_id_) {
    return true;
  }

  /// \brief removes block up to (but not including) a given height.
  /// \param[in] height_ height
  /// \param[out] pruned the number of blocks pruned.
  /// \return true on success, false otherwise
  bool prune_blocks(int64_t height_, uint64_t& pruned) {
    return true;
  }

private:
  std::shared_ptr<db_session_type> db_session_;
};

} // namespace noir::consensus