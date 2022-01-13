// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/state_store.h>

TEST_CASE("db_store", "[store]") {
  noir::consensus::db_store dbs("simple");
  noir::consensus::state st{};

  CHECK_NOTHROW(dbs.load(st));
  CHECK_NOTHROW(dbs.load_validators(0));
  CHECK_NOTHROW(dbs.load_abci_responses(0));
  CHECK_NOTHROW(dbs.load_consensus_params(0));
  CHECK_NOTHROW(dbs.save(st));
  CHECK_NOTHROW(dbs.save_abci_responses(st));
  CHECK_NOTHROW(dbs.save_validator_sets(st));
  CHECK_NOTHROW(dbs.bootstrap(st));
  CHECK_NOTHROW(dbs.prune_states(st));
}

TEST_CASE("block_store", "[store]") {
  noir::consensus::block_store bls("simple");
  CHECK_NOTHROW(bls.base());
  CHECK_NOTHROW(bls.height());
  CHECK_NOTHROW(bls.size());
  CHECK_NOTHROW(bls.load_block_meta(0));
  CHECK_NOTHROW(bls.load_block(0));
  CHECK_NOTHROW(bls.save_block(nullptr, nullptr, nullptr));
  CHECK_NOTHROW(bls.prune_blocks(0));

  CHECK_NOTHROW(bls.load_block_by_hash(noir::p2p::bytes{}));
  CHECK_NOTHROW(bls.load_block_part(0, 0));
  CHECK_NOTHROW(bls.load_block_commit(0));
  CHECK_NOTHROW(bls.load_seen_commit());
}
