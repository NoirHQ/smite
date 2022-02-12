// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/block_executor.h>
#include <noir/consensus/common_test.h>

using namespace noir;
using namespace noir::consensus;

std::tuple<state, std::shared_ptr<noir::consensus::db_store>, std::map<std::string, priv_validator>,
  std::shared_ptr<noir::db::session::session<noir::db::session::rocksdb_t>>>
make_state(int n_vals, int height) {
  std::vector<genesis_validator> vals;
  std::map<std::string, priv_validator> priv_vals;
  for (auto i = 0; i < n_vals; i++) {
    auto [val, priv_val] = rand_validator(false, 1000);
    vals.push_back(genesis_validator{val.address, val.pub_key_, val.voting_power, fmt::format("test#{}", i)});
    priv_vals[noir::to_hex(val.address)] = priv_val;
  }

  genesis_doc gen_doc;
  gen_doc.chain_id = "execution_chain";
  gen_doc.validators = vals;
  auto s = state::make_genesis_state(gen_doc);

  auto session = std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session());
  auto dbs = std::make_shared<noir::consensus::db_store>(session);
  dbs->save(s);

  for (auto i = 1; i < height; i++) {
    s.last_block_height++;
    s.last_validators = s.validators;
    dbs->save(s);
  }

  return {s, dbs, priv_vals, session};
};

TEST_CASE("Apply block", "[block_executor]") {
  auto [state_, state_db, priv_vals, session] = make_state(1, 1);

  auto proxyApp = std::make_shared<app_connection>();
  auto bls = std::make_shared<noir::consensus::block_store>(session);
  auto block_exec = block_executor::new_block_executor(state_db, proxyApp, bls);

  auto block_ = make_block(1, state_, commit{});
  auto block_id_ = p2p::block_id{block_.get_hash(), block_.make_part_set(65536).header()};

  block_exec->apply_block(state_, block_id_, block_);
}
