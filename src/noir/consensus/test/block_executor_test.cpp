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

std::tuple<state,
  std::shared_ptr<noir::consensus::db_store>,
  std::map<std::string, std::shared_ptr<priv_validator>>,
  std::shared_ptr<noir::db::session::session<noir::db::session::rocksdb_t>>>
make_state(int n_vals, int height) {
  std::vector<genesis_validator> vals;
  std::map<std::string, std::shared_ptr<priv_validator>> priv_vals;
  for (auto i = 0; i < n_vals; i++) {
    auto [val, priv_val] = rand_validator(false, 1000);
    vals.push_back(genesis_validator{val.address, val.pub_key_, val.voting_power, fmt::format("test#{}", i)});
    priv_vals[noir::to_hex(val.address)] = priv_val;
  }

  genesis_doc gen_doc;
  gen_doc.chain_id = "execution_chain";
  gen_doc.validators = vals;
  gen_doc.initial_height = 0;
  auto s = state::make_genesis_state(gen_doc);

  auto session = make_session();
  auto dbs = std::make_shared<noir::consensus::db_store>(session);
  dbs->save(s);

  for (auto i = 1; i < height; i++) {
    s.last_block_height++;
    s.last_validators = s.validators;
    dbs->save(s);
  }

  return {s, dbs, priv_vals, session};
};

TEST_CASE("block_executor: Apply block", "[noir][consensus]") {
  auto [state_, state_db, priv_vals, session] = make_state(1, 1);

  auto proxyApp = std::make_shared<app_connection>();
  auto bls = std::make_shared<noir::consensus::block_store>(session);
  auto ev_bus = std::make_shared<noir::consensus::events::event_bus>(app);
  auto [ev_pool, _] = ev::default_test_pool(1);
  auto block_exec = block_executor::new_block_executor(state_db, proxyApp, ev_pool, bls, ev_bus);

  auto block_ = ev::make_block(1, state_, std::make_shared<commit>());
  auto block_id_ = p2p::block_id{block_->get_hash(), block_->make_part_set(65536)->header()};

  CHECK(block_exec->apply_block(state_, block_id_, block_) != std::nullopt);
}
