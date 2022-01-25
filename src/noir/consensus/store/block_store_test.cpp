// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/store_test.h>

namespace {

TEST_CASE("block_store", "[block_store]") {
  noir::consensus::block_store bls(
    std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session()));

  CHECK_NOTHROW(bls.base());
  CHECK_NOTHROW(bls.height());
  CHECK_NOTHROW(bls.size());
  {
    noir::consensus::block_meta block_meta_{};
    CHECK_NOTHROW(bls.load_base_meta(block_meta_));
    CHECK_NOTHROW(bls.load_block_meta(0, block_meta_));
  }
  {
    noir::consensus::block bl_{};
    noir::consensus::part_set p_set_{};
    noir::consensus::commit commit_{};
    CHECK_NOTHROW(bls.save_block(bl_, p_set_, commit_));
    CHECK_NOTHROW(bls.load_block(0, bl_));

    CHECK_NOTHROW(bls.load_block_by_hash(noir::p2p::bytes{}, bl_));

    noir::consensus::part part_{};
    CHECK_NOTHROW(bls.load_block_part(0, 0, part_));
    CHECK_NOTHROW(bls.load_block_commit(0, commit_));
    CHECK_NOTHROW(bls.load_seen_commit(commit_));
  }
  {
    uint64_t pruned{0};
    CHECK_NOTHROW(bls.prune_blocks(0, pruned));
  }
}

} // namespace
