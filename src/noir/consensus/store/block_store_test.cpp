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

inline std::vector<noir::consensus::tx> make_txs(int64_t height, int num) {
  std::vector<noir::consensus::tx> txs{};
  for (auto i = 0; i < num; ++i) {
    txs.push_back(noir::consensus::tx{});
  }
  return txs;
}

inline noir::consensus::block make_block(
  int64_t height, const noir::consensus::state& st, const noir::consensus::commit& commit_) {
  auto txs = make_txs(st.last_block_height, 10);
  auto [block_, part_set_] = const_cast<noir::consensus::state&>(st).make_block(height, txs, commit_, /* {}, */ {});
  // TODO: temparary workaround to set block
  block_.header.height = height;
  return block_;
}

inline noir::consensus::part_set make_part_set(const noir::consensus::block& bl, uint32_t part_size) {
  // bl.mtx.lock();
  noir::p2p::part_set_header header_{
    .total = 1, // 4, // header, last_commit, data, evidence
  };
  auto p_set = noir::consensus::part_set::new_part_set_from_header(header_);

  p_set.add_part(noir::consensus::part{
    .index = 0,
    .bytes_ = noir::codec::scale::encode(bl.header), // .proof
  });
  // TODO: part_size & bl.data
  // bl.mtx.unlock();
  return p_set;
}

inline noir::consensus::commit make_commit(int64_t height, noir::p2p::tstamp timestamp) {
  static const std::string sig_s("Signature");
  noir::consensus::commit_sig sig_{
    .flag = noir::consensus::block_id_flag::FlagCommit,
    .validator_address = gen_random_bytes(32),
    .timestamp = timestamp,
    .signature = {sig_s.begin(), sig_s.end()},
  };
  return {
    .height = height,
    .my_block_id = {.hash = gen_random_bytes(32),
      .parts =
        {
          .total = 2,
          .hash = gen_random_bytes(32),
        }},
    .signatures = {sig_},
  };
}

inline noir::consensus::state make_genesis_state() {
  auto config_ = noir::consensus::config::default_config();
  config_.base.chain_id = "test_block_store";
  auto [gen_doc, priv_vals] = noir::consensus::rand_genesis_doc(config_, 1, false, 10);
  return noir::consensus::state::make_genesis_state(gen_doc);
}

TEST_CASE("base/height", "[block_store]") {
  noir::consensus::block_store bls(
    std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session()));
  SECTION("initialize") {
    CHECK(bls.base() == 0x0ll);
    CHECK(bls.height() == 0ll);
  }
}

TEST_CASE("save/load_block", "[block_store]") {
  noir::consensus::block_store bls(
    std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session()));
  auto genesis_state = make_genesis_state();

  // check there are no blocks at various heights
  noir::consensus::block bl_{};
  for (int i = -10; i < 1000; ++i) {
    CHECK(bls.load_block(i, bl_) == false);
  }

  noir::consensus::commit commit_{};
  for (auto height = 1; height < 100; ++height) {
    auto bl_ = make_block(height, genesis_state, commit_);
    auto hash_ = bl_.get_hash(); // TODO: temporary workaround (block get_hash() not implemented)
    auto p_set_ = make_part_set(bl_, 2);
    auto seen_commit_ = make_commit(10, noir::p2p::tstamp{});

    CHECK(bls.save_block(bl_, p_set_, seen_commit_) == true);
    CHECK(bls.base() == 1ll);
    CHECK(bls.height() == bl_.header.height);
    {
      noir::consensus::block ret{};
      CHECK(bls.load_block(height, ret) == true);
      CHECK(ret.get_hash() == bl_.get_hash());
    }
    {
      noir::consensus::part ret{};
      auto exp = p_set_.get_part(0);
      CHECK(bls.load_block_part(height, 0, ret) == true);
      CHECK(ret.index == exp.index);
      CHECK(ret.bytes_ == exp.bytes_);
      CHECK(ret.proof == exp.proof);
    }
    {
      noir::consensus::block_meta ret{};
      noir::consensus::block_meta exp{};
      CHECK(bls.load_block_meta(height, ret) == true);
      // TODO: enable below checks
      // CHECK(ret.bl_id == exp.bl_id);
      // CHECK(ret.bl_size == exp.bl_size);
      // CHECK(ret.header == exp.header);
      // CHECK(ret.num_txs == exp.num_txs);
    }
    {
      // TODO: bl.last_commit
      // noir::consensus::commit ret{};
      // noir::consensus::commit exp{};
      // CHECK(bls.load_block_commit(height - 1, ret) == true);
    }

    {
      // TODO: bl.get_hash()
      noir::consensus::block ret{};
      CHECK(bls.load_block_by_hash(hash_, ret) == true);
      CHECK(ret.get_hash() == bl_.get_hash());
    }

    {
      noir::consensus::block_meta ret{};
      noir::consensus::block_meta exp{};
      CHECK(bls.load_base_meta(ret) == true);
      // TODO: enable below checks
      // CHECK(ret.bl_id == exp.bl_id);
      // CHECK(ret.bl_size == exp.bl_size);
      // CHECK(ret.header == exp.header);
      // CHECK(ret.num_txs == exp.num_txs);
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

TEST_CASE("prune_block", "[block_store]") {
  noir::consensus::block_store bls(
    std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session()));
  auto genesis_state = make_genesis_state();

  // check there are no blocks at various heights
  noir::consensus::block bl_{};
  for (int i = -10; i < 1000; ++i) {
    CHECK(bls.load_block(i, bl_) == false);
  }

  noir::consensus::commit commit_{};
  for (auto height = 1; height <= 1500; ++height) {
    auto bl_ = make_block(height, genesis_state, commit_);
    auto p_set_ = make_part_set(bl_, 2);
    auto seen_commit_ = make_commit(10, noir::p2p::tstamp{});

    CHECK(bls.save_block(bl_, p_set_, seen_commit_) == true);
    CHECK(bls.base() == 1ll);
    CHECK(bls.height() == bl_.header.height);
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
