// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/common_test.h>
#include <noir/consensus/types/canonical.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("consensus_state: Proposer Selection 0", "[noir][consensus]") {
  auto local_config = config_setup();
  auto [cs1, vss] = rand_cs(local_config, 1);

  auto height = cs1->rs.height;
  auto round = cs1->rs.round;

  start_test_round(cs1, height, round);

  force_tick(cs1); // ensureNewRound

  auto rs = cs1->get_round_state();
  auto prop = rs->validators->get_proposer();
  auto pv = cs1->local_priv_validator->get_pub_key();

  auto addr = pv.address();

  CHECK(prop->address == addr);

  // wait for complete proposal // todo - how?
  force_tick(cs1); // ensureNewProposal

  rs = cs1->get_round_state();

  force_tick(cs1); // ensureNewProposal
  rs = cs1->get_round_state();

  //  cs1->schedule_timeout(std::chrono::milliseconds{3000}, 1, 0, NewHeight);
  //  cs1->schedule_timeout(std::chrono::milliseconds{4000}, 1, 1, Propose);
}

TEST_CASE("consensus_state: No Priv Validator", "[noir][consensus]") {
  auto local_config = config_setup();
  auto [cs1, vss] = rand_cs(local_config, 1);
  cs1->local_priv_validator = {};

  auto height = cs1->rs.height;
  auto round = cs1->rs.round;

  start_test_round(cs1, height, round);

  force_tick(cs1); // ensureNewRound

  CHECK(!cs1->get_round_state()->proposal);
}

TEST_CASE("consensus_state: Verify proposal signature", "[noir][consensus]") {
  auto local_config = config_setup();
  auto [cs1, vss] = rand_cs(local_config, 1);
  auto local_priv_validator = cs1->local_priv_validator;

  noir::p2p::proposal_message proposal_{};
  proposal_.timestamp = get_time();

  auto data_proposal1 = encode(canonical::canonicalize_proposal(proposal_));
  // std::cout << "data_proposal1=" << to_hex(data_proposal1) << std::endl;
  // std::cout << "digest1=" << fc::sha256::hash(data_proposal1).str() << std::endl;
  auto sig_org = local_priv_validator->sign_proposal(proposal_);
  // std::cout << "sig=" << std::string(proposal_.signature.begin(), proposal_.signature.end()) << std::endl;

  auto data_proposal2 = encode(canonical::canonicalize_proposal(proposal_));
  // std::cout << "data_proposal2=" << to_hex(data_proposal2) << std::endl;
  // std::cout << "digest2=" << fc::sha256::hash(data_proposal2).str() << std::endl;
  auto result = local_priv_validator->get_pub_key().verify_signature(data_proposal2, proposal_.signature);
  CHECK(result == true);
}

TEST_CASE("consensus_state: Verify vote signature", "[noir][consensus]") {
  auto local_config = config_setup();
  auto [cs1, vss] = rand_cs(local_config, 1);
  auto local_priv_validator = cs1->local_priv_validator;

  vote vote_{};
  vote_.timestamp = get_time();

  auto data_vote1 = encode(canonical::canonicalize_vote(vote_));
  // std::cout << "data_vote1=" << to_hex(data_vote1) << std::endl;
  // std::cout << "digest1=" << fc::sha256::hash(data_vote1).str() << std::endl;
  auto sig_org = local_priv_validator->sign_vote(vote_);
  // std::cout << "sig=" << std::string(vote_.signature.begin(), vote_.signature.end()) << std::endl;

  auto data_vote2 = encode(canonical::canonicalize_vote(vote_));
  // std::cout << "data_vote2=" << to_hex(data_vote2) << std::endl;
  // std::cout << "digest2=" << fc::sha256::hash(data_vote2).str() << std::endl;
  auto result = local_priv_validator->get_pub_key().verify_signature(data_vote2, vote_.signature);
  CHECK(result == true);
}

TEST_CASE("consensus_state: Test State Full Round1", "[noir][consensus]") {
  appbase::application app_;
  app_.register_plugin<test_plugin>();
  app_.initialize<test_plugin>();

  auto local_config = config_setup();
  constexpr int timeout_new_round = 3; // TODO: check this time is correct
  constexpr int timeout_propose = 3;
  constexpr int timeout_prevote = 1;
  constexpr int timeout_precommit = 1;
  constexpr int timeout_commit = 3;
  constexpr int timeout_delta = 1;

  local_config.consensus.timeout_propose = std::chrono::seconds{timeout_propose};
  local_config.consensus.timeout_prevote = std::chrono::seconds{timeout_prevote};
  local_config.consensus.timeout_precommit = std::chrono::seconds{timeout_precommit};
  local_config.consensus.timeout_commit = std::chrono::seconds{timeout_commit};

  auto [cs1, vss] = rand_cs(local_config, 1, app_);
  auto local_priv_validator = cs1->local_priv_validator;
  auto cs_monitor = status_monitor("test", cs1->event_bus_, cs1);
  auto height = cs1->rs.height;
  auto round = cs1->rs.round;

  auto thread = std::make_unique<noir::named_thread_pool>("test_thread", 5);
  auto res = noir::async_thread_pool(thread->get_executor(), [&]() {
    app_.startup();
    app_.exec();
  });

  auto event_message_filter = [](const events::message& msg) {
    static const std::vector<int> subscribed_msg_types = {
      status_monitor::get_message_type_index<events::event_data_vote>(),
      status_monitor::get_message_type_index<events::event_data_complete_proposal>(),
      status_monitor::get_message_type_index<events::event_data_new_round>(),
    };
    return std::any_of(subscribed_msg_types.begin(), subscribed_msg_types.end(),
      [&](int i) { return i == static_cast<int>(msg.data.index()); });
  };

  cs_monitor.subscribe_filtered_msg(event_message_filter);
  start_test_round(cs1, height, round);

  CHECK(cs_monitor.ensure_new_round(timeout_new_round + timeout_delta, height, round) == true);
  CHECK(cs_monitor.ensure_new_proposal(timeout_propose + timeout_delta, height, round) == true);
  auto prop_block_hash = cs1->get_round_state()->proposal_block->get_hash();
  CHECK(cs_monitor.ensure_prevote(timeout_prevote + timeout_delta, height, round) == true);

  CHECK(validate_prevote(*cs1, round, vss[0], prop_block_hash) == true);
  CHECK(cs_monitor.ensure_precommit(timeout_precommit + timeout_delta, height, round) == true);
  CHECK(cs_monitor.ensure_new_round(timeout_commit + timeout_delta, height + 1, 0) == true);
  CHECK(validate_last_precommit(*cs1, vss[0], prop_block_hash) == true);

  app_.quit();
}
