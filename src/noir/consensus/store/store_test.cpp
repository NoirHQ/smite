// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/state_store.h>

TEST_CASE("save/load validator_set", "[db_store]") {
  noir::consensus::db_store dbs("simple");

  std::vector<noir::consensus::validator> validator_list;
  validator_list.push_back(noir::consensus::validator{
    .address = noir::p2p::bytes{0, 1, 2, 3},
    .voting_power = 1,
  });
  auto v_set = noir::consensus::validator_set::new_validator_set(validator_list);
  noir::consensus::validator_set ret{};

  CHECK(dbs.save_validator_sets(1, 2, v_set) == true);
  CHECK(dbs.load_validators(1, ret) == true);
  CHECK(v_set.validators[0].address == ret.validators[0].address);
  CHECK(v_set.validators[0].voting_power == ret.validators[0].voting_power);
  CHECK(v_set.validators[0].proposer_priority == ret.validators[0].proposer_priority);
  CHECK(dbs.load_validators(2, ret) == true);
  CHECK(v_set.validators[0].address == ret.validators[0].address);
  CHECK(v_set.validators[0].voting_power == ret.validators[0].voting_power);
  CHECK(v_set.validators[0].proposer_priority == ret.validators[0].proposer_priority);
}

TEST_CASE("save/load consensus_param", "[db_store]") {
  noir::consensus::db_store dbs("simple");
  noir::consensus::consensus_params cs_param{};

  CHECK_NOTHROW(dbs.load_consensus_params(0, cs_param));
}

TEST_CASE("save/load abci_response", "[db_store]") {
  noir::consensus::db_store dbs("simple");
  //  noir::consensus::abci_response rsp{};

  bool ret;
  CHECK_NOTHROW(ret = dbs.save_abci_responses(0 /* , rsp */));
  CHECK_NOTHROW(ret = dbs.load_abci_responses(0 /* , rsp */));
}

TEST_CASE("save/load state", "[db_store]") {
  noir::consensus::db_store dbs("simple");
  noir::consensus::state st{};
  st.last_height_validators_changed = 0;
  noir::consensus::state ret{};

  CHECK(dbs.save(st) == true);
  CHECK(dbs.load(ret) == true);

  CHECK(st.version == ret.version);
  CHECK(st.chain_id == ret.chain_id);
  CHECK(st.initial_height == ret.initial_height);
  CHECK(st.last_block_height == ret.last_block_height);
  //  CHECK(st.last_block_id == ret.last_block_id);
  CHECK(st.last_block_time == ret.last_block_time);
  //  CHECK(st.next_validators == ret.next_validators);
  //  CHECK(st.validators == ret.validators);
  //  CHECK(st.last_validators == ret.last_validators);
  CHECK(st.last_height_validators_changed == ret.last_height_validators_changed);
  //  CHECK(st.consensus_params == ret.consensus_params);
  CHECK(st.last_height_consensus_params_changed == ret.last_height_consensus_params_changed);
  CHECK(st.last_result_hash == ret.last_result_hash);
  //  CHECK(st.app_hash == ret.app_hash);
}

TEST_CASE("bootstrap", "[db_store]") {
  noir::consensus::db_store dbs("simple");
  noir::consensus::state st{};
  st.last_block_height = 1;
  st.initial_height = 0;
  st.last_height_validators_changed = 0;
  noir::consensus::state ret{};

  CHECK_NOTHROW(dbs.bootstrap(st));
  CHECK(dbs.load(ret) == true);

  CHECK(st.version == ret.version);
  CHECK(st.chain_id == ret.chain_id);
  CHECK(st.initial_height == ret.initial_height);
  CHECK(st.last_block_height == ret.last_block_height);
  //  CHECK(st.last_block_id == ret.last_block_id);
  CHECK(st.last_block_time == ret.last_block_time);
  //  CHECK(st.next_validators == ret.next_validators);
  //  CHECK(st.validators == ret.validators);
  //  CHECK(st.last_validators == ret.last_validators);
  CHECK(st.last_height_validators_changed == ret.last_height_validators_changed);
  //  CHECK(st.consensus_params == ret.consensus_params);
  CHECK(st.last_height_consensus_params_changed == ret.last_height_consensus_params_changed);
  CHECK(st.last_result_hash == ret.last_result_hash);
  //  CHECK(st.app_hash == ret.app_hash);
}

TEST_CASE("prune_state", "[db_store]") {
  struct test_case {
    std::string name;
    int64_t start_height;
    int64_t end_height;
    int64_t prune_height;
    bool expect_err;
    int64_t remaining_val_set_height;
    int64_t remaining_params_set_height;
  };
  auto tests = std::to_array<struct test_case>({
    test_case{"error when prune height is 0", 1, 100, 0, true, 0, 0},
    test_case{"error when prune height is negative", 1, 100, -10, true, 0, 0},
    test_case{"error when prune height does not exist", 1, 100, 101, true, 0, 0},
    test_case{"prune all", 1, 100, 100, false, 93, 95},
    test_case{"prune from non 1 height", 10, 50, 40, false, 33, 35},
    test_case{"prune some", 1, 10, 8, false, 3, 5},
    test_case{"prune more than 1000 state", 1, 1010, 1010, false, 1003, 1005},
    test_case{"prune across checkpoint", 99900, 100002, 100002, false, 100000, 99995},
  });

  std::for_each(tests.begin(), tests.end(), [&](const test_case& t) {
    SECTION(t.name) {
      noir::consensus::db_store dbs("simple");

      std::vector<noir::consensus::validator> validator_list;
      validator_list.push_back(noir::consensus::validator{
        .address = noir::p2p::bytes{0, 1, 2, 3},
        .voting_power = 1,
      });
      auto v_set = noir::consensus::validator_set::new_validator_set(validator_list);
      int64_t vals_changed(0);
      int64_t params_changed(0);

      // prepare test
      for (auto height = t.start_height; height <= t.end_height; ++height) {
        if (vals_changed == 0 || height % 10 == 2) {
          vals_changed = height + 1;
        }
        if (params_changed == 0 || height % 10 == 5) {
          params_changed = height;
        }

        noir::consensus::state st{};
        st.initial_height = 1;
        st.last_block_height = height - 1;
        st.validators = v_set;
        st.next_validators = v_set;
        st.consensus_params = noir::consensus::consensus_params{
          .block{10000000},
        };
        st.last_height_validators_changed = vals_changed;
        st.last_height_consensus_params_changed = params_changed;

        if (st.last_block_height >= 1) {
          st.last_validators = st.validators;
        }

        CHECK(dbs.save(st) == true);
        CHECK(dbs.save_abci_responses(height /*, abci_rsp */) == true);
      }

      // start test
      {
        auto ret = dbs.prune_states(t.prune_height);
        if (t.expect_err) {
          CHECK(ret == false);
          return;
        }
        CHECK(ret == true);
      }

      for (auto height = t.prune_height; height <= t.end_height; ++height) {
        noir::consensus::validator_set vals{};
        CHECK(dbs.load_validators(height, vals) == true);
        CHECK(vals.size() != 0); // nil check

        noir::consensus::consensus_params cs_param{};
        CHECK(dbs.load_consensus_params(height, cs_param) == true);
        // CHECK(cs_param != noir::consensus::consensus_params{}); // nil check

        CHECK(dbs.load_abci_responses(height /* , abci_rsp */) == true);
        // CHECK(abci_rsp != std::nullopt) // nil check
      }

      noir::consensus::consensus_params empty_cs_param{};
      for (auto height = t.start_height; height < t.prune_height; ++height) {
        noir::consensus::validator_set vals{};
        {
          auto ret = dbs.load_validators(height, vals);
          if (height == t.remaining_val_set_height) {
            CHECK(ret == true);
            CHECK(vals.size() != 0); // nil check
          } else {
            CHECK(ret == false);
            CHECK(vals.size() == 0); // nil check
          }
        }

        {
          noir::consensus::consensus_params cs_param{};
          auto ret = dbs.load_consensus_params(height, cs_param);
          if (height == t.remaining_params_set_height) {
            CHECK(ret == true);
            // CHECK(cs_param != noir::consensus::consensus_params{}); // nil check
          } else {
            CHECK(ret == false);
            // CHECK(cs_param == noir::consensus::consensus_params{}); // nil check
          }
        }

        CHECK_NOTHROW(dbs.load_abci_responses(height /* , abci_rsp */));
        // CHECK(abci_rsp == std::nullopt) // nil check
      }
    }
  });
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
