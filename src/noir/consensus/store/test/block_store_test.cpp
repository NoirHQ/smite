// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/common_test.h>
#include <noir/consensus/state.h>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/store_test.h>

namespace {

static noir::consensus::state make_genesis_state() {
  auto config_ = noir::consensus::config::get_default();
  config_.base.chain_id = "test_block_store";
  config_.base.root_dir = "/tmp/test_block_store";
  config_.consensus.root_dir = config_.base.root_dir;
  config_.priv_validator.root_dir = config_.base.root_dir;
  auto [gen_doc, priv_vals] = noir::consensus::rand_genesis_doc(config_, 1, false, 10);
  return noir::consensus::state::make_genesis_state(gen_doc);
}

static inline void comp_hdr(noir::consensus::block_header& ret_hdr, noir::consensus::block_header& exp_hdr) {
  CHECK(ret_hdr.version == exp_hdr.version);
  CHECK(ret_hdr.chain_id == exp_hdr.chain_id);
  CHECK(ret_hdr.height == exp_hdr.height);
  CHECK(ret_hdr.time == exp_hdr.time);
  CHECK(ret_hdr.last_block_id == exp_hdr.last_block_id);
  CHECK(ret_hdr.last_commit_hash == exp_hdr.last_commit_hash);
  CHECK(ret_hdr.data_hash == exp_hdr.data_hash);
  CHECK(ret_hdr.validators_hash == exp_hdr.validators_hash);
  CHECK(ret_hdr.next_validators_hash == exp_hdr.next_validators_hash);
  CHECK(ret_hdr.consensus_hash == exp_hdr.consensus_hash);
  CHECK(ret_hdr.app_hash == exp_hdr.app_hash);
  CHECK(ret_hdr.last_results_hash == exp_hdr.last_results_hash);
  CHECK(ret_hdr.proposer_address == exp_hdr.proposer_address);
  CHECK(ret_hdr.get_hash() == exp_hdr.get_hash());
};

TEST_CASE("block_store: base/height", "[noir][consensus]") {
  noir::consensus::block_store bls(make_session());
  SECTION("initialize") {
    CHECK(bls.base() == 0x0ll);
    CHECK(bls.height() == 0ll);
  }
}

TEST_CASE("block_store: save/load_block", "[noir][consensus]") {
  noir::consensus::block_store bls(make_session());
  auto genesis_state = make_genesis_state();

  // check there are no blocks at various heights
  noir::consensus::block bl_{};
  for (int i = -10; i < 1000; ++i) {
    CHECK(bls.load_block(i, bl_) == false);
  }

  noir::consensus::block_meta base_meta{};
  noir::consensus::commit commit_{};
  for (auto height = 1; height < 100; ++height) {
    auto bl_ = make_block(height, genesis_state, commit_);
    auto p_set_ = bl_->make_part_set(64); // make parts more than one
    auto seen_commit_ = noir::consensus::make_commit(10, noir::p2p::tstamp{});
    REQUIRE(bl_ != nullptr);
    REQUIRE(p_set_ != nullptr);

    CHECK(bls.save_block(*bl_, *p_set_, seen_commit_) == true);
    CHECK(bls.base() == 1ll);
    if (height == bls.base()) {
      base_meta = noir::consensus::block_meta::new_block_meta(*bl_, *p_set_);
    }
    CHECK(bls.height() == bl_->header.height);
    {
      noir::consensus::block ret{};
      CHECK(bls.load_block(height, ret) == true);
      CHECK(ret.get_hash() == bl_->get_hash());
    }
    {
      noir::consensus::part ret{};
      auto exp = p_set_->get_part(0);
      CHECK(bls.load_block_part(height, 0, ret) == true);
      CHECK(ret.index == exp->index);
      CHECK(ret.bytes_ == exp->bytes_);
      // CHECK(ret.proof_ == exp->proof_); // TODO: enable later
    }
    {
      noir::consensus::block_meta ret{};
      auto exp = noir::consensus::block_meta::new_block_meta(*bl_, *p_set_);

      CHECK(bls.load_block_meta(height, ret) == true);
      CHECK(ret.bl_id == exp.bl_id);
      CHECK(ret.bl_size == exp.bl_size);
      comp_hdr(ret.header, exp.header);
      CHECK(ret.num_txs == exp.num_txs);
    }
    {
      noir::consensus::commit ret{};
      CHECK(bls.load_block_commit(height - 1, ret) == true);
      CHECK(ret.get_hash() == bl_->last_commit.get_hash());
    }

    {
      noir::consensus::block ret{};
      CHECK(bls.load_block_by_hash(bl_->get_hash(), ret) == true);
      CHECK(ret.get_hash() == bl_->get_hash());
    }

    {
      noir::consensus::block_meta ret{};
      auto& exp = base_meta;
      CHECK(bls.load_base_meta(ret) == true);
      CHECK(ret.bl_id == exp.bl_id);
      CHECK(ret.bl_size == exp.bl_size);
      comp_hdr(ret.header, exp.header);
      CHECK(ret.num_txs == exp.num_txs);
    }
    {
      noir::consensus::commit ret{};
      noir::consensus::commit& exp = seen_commit_;
      CHECK(bls.load_seen_commit(ret) == true);
      CHECK(ret.height == exp.height);
      CHECK(ret.round == exp.round);
      CHECK(ret.my_block_id == exp.my_block_id);
      // CHECK(ret.signatures == exp.signatures);
    }
  }
}

TEST_CASE("block_store: prune_block", "[noir][consensus]") {
  noir::consensus::block_store bls(make_session());
  auto genesis_state = make_genesis_state();

  // check there are no blocks at various heights
  noir::consensus::block bl_{};
  for (int i = -10; i < 1000; ++i) {
    CHECK(bls.load_block(i, bl_) == false);
  }

  noir::consensus::commit commit_{};
  for (auto height = 1; height <= 1500; ++height) {
    auto bl_ = make_block(height, genesis_state, commit_);
    auto p_set_ = bl_->make_part_set(64); // make parts more than one
    auto seen_commit_ = noir::consensus::make_commit(10, noir::p2p::tstamp{});

    CHECK(bls.save_block(*bl_, *p_set_, seen_commit_) == true);
    CHECK(bls.base() == 1ll);
    CHECK(bls.height() == bl_->header.height);
  }

  noir::consensus::block pruned_block{};
  CHECK(bls.load_block(1200, pruned_block) == true);
  uint64_t num_pruned{0};

  // prune more than 1000 blocks, to test batch deletions
  CHECK(bls.prune_blocks(1200, num_pruned) == true);
  CHECK(num_pruned == 1199ull);
  CHECK(bls.base() == 1200ll);
  CHECK(bls.height() == 1500ll);

  auto check_block = [&](int start, int prune, int max, int skip) {
    noir::consensus::block tmp_block{};
    for (auto i = start; i < prune; i += skip) {
      CHECK(bls.load_block(i, tmp_block) == false);
    }
    for (auto i = prune; i < max; i += skip) {
      CHECK(bls.load_block(i, tmp_block) == true);
    }
  };
  check_block(1, 1200, 1500, 10);

  // Pruning below the current base should not error
  CHECK(bls.prune_blocks(1000, num_pruned) == true);
  CHECK(num_pruned == 0ull);
  CHECK(bls.base() == 1200ll);
  CHECK(bls.height() == 1500ll);

  // Pruning to the current base should not error
  CHECK(bls.prune_blocks(1200, num_pruned) == true);
  CHECK(num_pruned == 0ull);
  CHECK(bls.base() == 1200ll);
  CHECK(bls.height() == 1500ll);

  // Pruning again should work
  CHECK(bls.prune_blocks(1300, num_pruned) == true);
  CHECK(num_pruned == 100ull);
  CHECK(bls.base() == 1300ll);
  CHECK(bls.height() == 1500ll);
  check_block(1200, 1300, 1500, 1);

  // Pruning beyond the current height should error
  CHECK(bls.prune_blocks(1501, num_pruned) == false);

  // Pruning to the current height should work
  CHECK(bls.prune_blocks(1500, num_pruned) == true);
  CHECK(num_pruned == 200ull);
  CHECK(bls.base() == 1500ll);
  CHECK(bls.height() == 1500ll);
  check_block(1300, 1500, 1500, 1);
}

} // namespace
