// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/store/state_store.h>
#include <noir/consensus/store/store_test.h>

namespace {

inline void check_state_equal(noir::consensus::state& lhs, noir::consensus::state& rhs) {
  CHECK(lhs.version == rhs.version);
  CHECK(lhs.chain_id == rhs.chain_id);
  CHECK(lhs.initial_height == rhs.initial_height);
  CHECK(lhs.last_block_height == rhs.last_block_height);
  CHECK(lhs.last_block_id == rhs.last_block_id);
  CHECK(lhs.last_block_time == rhs.last_block_time);
  // CHECK(lhs.next_validators == rhs.next_validators);
  // CHECK(lhs.validators == rhs.validators);
  // CHECK(lhs.last_validators == rhs.last_validators);
  CHECK(lhs.last_height_validators_changed == rhs.last_height_validators_changed);
  // CHECK(lhs.consensus_params == rhs.consensus_params);
  CHECK(lhs.last_height_consensus_params_changed == rhs.last_height_consensus_params_changed);
  CHECK(lhs.last_result_hash == rhs.last_result_hash);
  CHECK(lhs.app_hash == rhs.app_hash);
}

TEST_CASE("db_store: save/load validator_set", "[noir][consensus]") {
  noir::consensus::db_store dbs(make_session());

  std::vector<noir::consensus::validator> validator_list;
  validator_list.push_back(noir::consensus::validator{
    .address = gen_random_bytes(32),
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

TEST_CASE("db_store: save/load consensus_param", "[noir][consensus]") {
  noir::consensus::db_store dbs(make_session());
  noir::consensus::consensus_params cs_param{};

  CHECK_NOTHROW(dbs.load_consensus_params(0, cs_param));
}

TEST_CASE("db_store: save/load abci_response", "[noir][consensus]") {
  noir::consensus::db_store dbs(make_session());
  noir::consensus::abci_responses rsp{};

  bool ret;
  CHECK_NOTHROW(ret = dbs.save_abci_responses(0, rsp));
  CHECK_NOTHROW(ret = dbs.load_abci_responses(0, rsp));
}

TEST_CASE("db_store: save/load state", "[noir][consensus]") {
  noir::consensus::db_store dbs(make_session());
  noir::consensus::state st{};
  st.last_height_validators_changed = 0;
  noir::consensus::state ret{};

  CHECK(dbs.save(st) == true);
  CHECK(dbs.load(ret) == true);
  check_state_equal(st, ret);
}

TEST_CASE("db_store: bootstrap", "[noir][consensus]") {
  noir::consensus::db_store dbs(make_session());
  noir::consensus::state st{};
  st.last_block_height = 1;
  st.initial_height = 0;
  st.last_height_validators_changed = 0;
  noir::consensus::state ret{};

  CHECK_NOTHROW(dbs.bootstrap(st));
  CHECK(dbs.load(ret) == true);
  check_state_equal(st, ret);
}

TEST_CASE("db_store: prune_state", "[noir][consensus]") {
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
      static int test_num = 0;
      noir::consensus::db_store dbs(make_session());

      std::vector<noir::consensus::validator> validator_list;
      validator_list.push_back(noir::consensus::validator{
        .address = gen_random_bytes(32),
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
        st.consensus_params_ = noir::consensus::consensus_params{
          .block{10000000},
        };
        st.last_height_validators_changed = vals_changed;
        st.last_height_consensus_params_changed = params_changed;

        if (st.last_block_height >= 1) {
          st.last_validators = st.validators;
        }

        noir::consensus::abci_responses abci_rsp{};
        CHECK(dbs.save(st) == true);
        CHECK(dbs.save_abci_responses(height, abci_rsp) == true);
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

        noir::consensus::abci_responses abci_ret{};
        CHECK(dbs.load_abci_responses(height, abci_ret) == true);
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

        noir::consensus::abci_responses abci_ret{};
        CHECK_NOTHROW(dbs.load_abci_responses(height, abci_ret));
      }
    }
  });
}

} // namespace
