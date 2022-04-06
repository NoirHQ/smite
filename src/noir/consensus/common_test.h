// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/go.h>
#include <noir/common/hex.h>
#include <noir/common/overloaded.h>
#include <noir/consensus/config.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/store/store_test.h>
#include <noir/consensus/types.h>

#include <appbase/application.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/private_key.hpp>
#include <fc/crypto/public_key.hpp>
#include <fc/crypto/signature.hpp>

#include <stdlib.h>

appbase::application app;

namespace noir::consensus {

class test_plugin : public appbase::plugin<test_plugin> {
public:
  APPBASE_PLUGIN_REQUIRES()

  virtual ~test_plugin() {}

  void set_program_options(CLI::App& app_config) {}

  void plugin_initialize(const CLI::App& app_config) {}

  void plugin_startup() {}

  void plugin_shutdown() {}
};

constexpr int64_t test_min_power = 10;

struct validator_stub {
  int32_t index;
  int64_t height;
  int32_t round;
  std::shared_ptr<priv_validator> priv_val;
  int64_t voting_power;
  vote last_vote;

  static validator_stub new_validator_stub(std::shared_ptr<priv_validator> priv_val_, int32_t val_index) {
    return validator_stub{val_index, 0, 0, priv_val_, test_min_power};
  }
};

using validator_stub_list = std::vector<validator_stub>;

void increment_height(validator_stub_list& vss, size_t begin_at) {
  for (auto it = vss.begin() + begin_at; it != vss.end(); it++)
    it->height++;
}

config config_setup() {
  auto config_ = config::get_default();
  config_.base.chain_id = "test_chain";

  // create consensus.root_dir if not exist
  auto temp_dir = std::filesystem::temp_directory_path();
  auto temp_config_path = temp_dir / "test_consensus";
  std::filesystem::create_directories(temp_config_path);

  config_.base.root_dir = temp_config_path.string();
  config_.consensus.root_dir = config_.base.root_dir;
  config_.priv_validator.root_dir = config_.base.root_dir;
  return config_;
}

std::tuple<validator, std::shared_ptr<priv_validator>> rand_validator(bool rand_power, int64_t min_power) {
  static int i = 0;
  auto priv_val = std::make_shared<mock_pv>();

  // Randomly generate a private key // TODO: use a different PKI algorithm
  auto new_key = fc::crypto::private_key::generate<fc::ecc::private_key_shim>(); // randomly generate a private key
  //  fc::crypto::private_key new_key("5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"); // TODO: read from a file
  priv_val->priv_key_.key = fc::from_base58(new_key.to_string());
  priv_val->pub_key_ = priv_val->priv_key_.get_pub_key();

  auto vote_power = min_power;
  if (rand_power)
    vote_power += std::rand();
  return {validator{priv_val->pub_key_.address(), priv_val->pub_key_, vote_power, 0}, priv_val};
}

std::tuple<genesis_doc, std::vector<std::shared_ptr<priv_validator>>> rand_genesis_doc(
  const config& config_, int num_validators, bool rand_power, int64_t min_power) {
  std::vector<genesis_validator> validators;
  std::vector<std::shared_ptr<priv_validator>> priv_validators;
  for (auto i = 0; i < num_validators; i++) {
    auto [val, priv_val] = rand_validator(rand_power, min_power);
    validators.push_back(genesis_validator{val.address, val.pub_key_, val.voting_power});
    priv_validators.push_back(priv_val);
  }
  return {genesis_doc{get_time(), config_.base.chain_id, 1, {}, validators}, priv_validators};
}

std::tuple<state, std::vector<std::shared_ptr<priv_validator>>> rand_genesis_state(
  config& config_, int num_validators, bool rand_power, int64_t min_power) {
  auto [gen_doc, priv_vals] = rand_genesis_doc(config_, num_validators, rand_power, min_power);
  auto state_ = state::make_genesis_state(gen_doc);
  return {state_, priv_vals};
}

std::tuple<std::shared_ptr<consensus_state>, validator_stub_list> rand_cs(
  config& config_, int num_validators, appbase::application& app_ = app) {
  auto [state_, priv_vals] = rand_genesis_state(config_, num_validators, false, 10);

  validator_stub_list vss;

  auto db_dir = std::filesystem::path{config_.consensus.root_dir} / "testdb";
  auto session = make_session(true, db_dir);
  auto dbs = std::make_shared<noir::consensus::db_store>(session);
  auto proxyApp = std::make_shared<app_connection>();
  auto bls = std::make_shared<noir::consensus::block_store>(session);
  auto ev_bus = std::make_shared<noir::consensus::events::event_bus>(app_);
  auto block_exec = block_executor::new_block_executor(dbs, proxyApp, bls, ev_bus);

  auto cs = consensus_state::new_state(app_, config_.consensus, state_, block_exec, bls, ev_bus);
  cs->set_priv_validator(priv_vals[0]); // todo - requires many other fields to be properly initialized

  for (auto i = 0; i < num_validators; i++) {
    vss.push_back(validator_stub{i, 0, 0, priv_vals[i], test_min_power});
  }

  increment_height(vss, 1);

  return {move(cs), vss};
}

void start_test_round(std::shared_ptr<consensus_state>& cs, int64_t height, int32_t round) {
  cs->on_start();
  cs->enter_new_round(height, round);
  //  cs.startRoutines(0) // not needed for noir
}

void force_tick(std::shared_ptr<consensus_state>& cs) {
  cs->timeout_ticker_timer->cancel(); // forces tick
}

void sign_add_votes(config& config_, std::shared_ptr<consensus_state>& cs, p2p::signed_msg_type type, bytes hash,
  p2p::part_set_header header) {}

bool validate_votes(const std::shared_ptr<vote_set>& votes, const bytes& address, const bytes& block_hash) {
  auto index = votes->val_set.get_index_by_address(address);
  if (index < 0) {
    return false;
  }

  auto& vote = votes->votes[index];
  if (!vote) {
    return false;
  }
  return vote->block_id_.hash == block_hash;
}

bool validate_prevote(consensus_state& cs_state, int32_t round, validator_stub& priv_val, bytes& block_hash) {
  auto prevotes = cs_state.rs.votes->prevotes(round);
  auto pub_key = priv_val.priv_val->get_pub_key();
  auto address = pub_key.address();
  return validate_votes(prevotes, address, block_hash);
}

bool validate_last_precommit(consensus_state& cs_state, validator_stub& priv_val, bytes& block_hash) {
  auto votes = cs_state.rs.last_commit;
  auto pub_key = priv_val.priv_val->get_pub_key();
  auto address = pub_key.address();
  return validate_votes(votes, address, block_hash);
}

inline std::vector<noir::consensus::tx> make_txs(int64_t height, int num) {
  std::vector<noir::consensus::tx> txs{};
  for (auto i = 0; i < num; ++i) {
    txs.push_back(noir::consensus::tx{});
  }
  return txs;
}

inline std::shared_ptr<noir::consensus::block> make_block(
  int64_t height, const noir::consensus::state& st, const noir::consensus::commit& commit_) {
  auto txs = make_txs(st.last_block_height, 10);
  auto [block_, part_set_] = const_cast<noir::consensus::state&>(st).make_block(height, txs, commit_, /* {}, */ {});
  // TODO: temparary workaround to set block
  block_->header.height = height;
  return block_;
}

inline std::shared_ptr<noir::consensus::part_set> make_part_set(const noir::consensus::block& bl, uint32_t part_size) {
  // bl.mtx.lock();
  noir::p2p::part_set_header header_{
    .total = 1, // 4, // header, last_commit, data, evidence
  };
  auto p_set = noir::consensus::part_set::new_part_set_from_header(header_);

  p_set->add_part(std::make_shared<noir::consensus::part>(noir::consensus::part{
    .index = 0,
    .bytes_ = encode(bl.header), // .proof
  }));
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

class status_monitor {
public:
  status_monitor() = default;
  status_monitor(status_monitor&&) = default;
  status_monitor& operator=(status_monitor&&) = default;
  status_monitor(const std::string& node_name, const std::shared_ptr<events::event_bus>& ev_bus,
    const std::shared_ptr<consensus_state>& cs_state)
    : node_name(node_name), ev_bus(ev_bus), cs_state_(cs_state) {}
  // copy is not permitted
  status_monitor(const status_monitor&) = delete;
  status_monitor& operator=(const status_monitor&) = delete;

  template<typename FilterCb>
  void subscribe_filtered_msg(FilterCb cb) {
    std::shared_ptr<events::event_bus> ev{ev_bus.lock()};
    REQUIRE(ev != nullptr);
    handle = ev->subscribe(node_name, [&](const events::message msg) {
      if (cb(msg)) {
        msg_queue.push(msg);
      }
    });
  }

  struct process_msg_result {
    std::string node_name;
    int32_t index = -1;
    int64_t height = -1;
    int32_t round = -1;
    p2p::round_step_type step = static_cast<p2p::round_step_type>(-1);
    p2p::signed_msg_type vote_type = p2p::signed_msg_type::Unknown;

    std::string to_string() {
      return fmt::format("test_node[{}] type_index={} height={} round={} step={}", node_name, index, height, round,
        p2p::round_step_to_str(step));
    }
  };

  process_msg_result process_msg() {
    process_msg_result ret{
      .node_name = node_name,
    };
    if (msg_queue.empty()) {
      return ret;
    }
    auto state = cs_state_.lock();
    REQUIRE(state != nullptr);

    auto& event_msg = msg_queue.front();
    noir_defer([&msg_queue = msg_queue]() { msg_queue.pop(); });
    ret.index = event_msg.data.index();

    std::visit(noir::overloaded{
                 [&](const events::event_data_new_block& msg) {
                   // std::cout << "event_data_new_block received!!" << std::endl;
                 },
                 [&](const events::event_data_new_block_header& msg) {
                   // std::cout << "event_data_new_block_header received!!" << std::endl;
                 },
                 [&](const events::event_data_new_evidence& msg) {
                   // std::cout << "event_data_new_evidence received!!" << std::endl;
                 },
                 [&](const events::event_data_tx& msg) {
                   // std::cout << "event_data_tx received!!" << std::endl;
                 },
                 [&](const events::event_data_new_round& msg) {
                   // std::cout << "event_data_new_round received!!" << std::endl;
                   ret.height = msg.height;
                   ret.round = msg.round;
                   ret.step = step_map_[msg.step];
                 },
                 [&](const events::event_data_complete_proposal& msg) {
                   // std::cout << "event_data_complete_proposal received!!" << std::endl;
                   ret.height = msg.height;
                   ret.round = msg.round;
                   ret.step = step_map_[msg.step];
                 },
                 [&](const events::event_data_vote& msg) {
                   // std::cout << "event_data_vote received!!" << std::endl;
                   ret.height = msg.vote.height;
                   ret.round = msg.vote.round;
                   ret.vote_type = msg.vote.type;
                 },
                 [&](const events::event_data_string& str) {
                   // std::cout << "event_data_string received!!" << std::endl;
                 },
                 [&](const events::event_data_validator_set_updates& msg) {
                   // std::cout << "event_data_new_block_header received!!" << std::endl;
                 },
                 [&](const events::event_data_block_sync_status& msg) {
                   // std::cout << "event_data_block_sync_status received!!" << std::endl;
                 },
                 [&](const events::event_data_state_sync_status& msg) {
                   // std::cout << "event_data_state_sync_status received!!" << std::endl;
                 },
                 [&](const events::event_data_round_state& msg) {
                   // std::cout << "event_data_round_state received!!" << std::endl;
                   ret.height = msg.height;
                   ret.round = msg.round;
                   ret.step = step_map_[msg.step];
                 },
                 [&](const auto& msg) {
                   std::cout << "invalid msg type" << std::endl;
                   CHECK(false);
                 },
               },
      event_msg.data);

    return ret;
  }

  bool ensure_new_event(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_round_state>(timeout, height, round);
  }

  bool ensure_new_round(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_new_round>(timeout, height, round);
  }

  bool ensure_new_timeout(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_round_state>(timeout, height, round);
  }

  bool ensure_new_proposal(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_complete_proposal>(timeout, height, round);
  }

  bool ensure_new_valid_block(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_round_state>(timeout, height, round);
  }

  bool ensure_new_block(int timeout, int64_t height) {
    return ensure_events_internal<events::event_data_new_block>(timeout, height);
  }

  bool ensure_new_block_header(int timeout, int64_t height) {
    return ensure_events_internal<events::event_data_new_block_header>(timeout, height);
  }

  bool ensure_new_unlock(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_round_state>(timeout, height, round);
  }

  bool ensure_proposal(int timeout, int64_t height, int32_t round) {
    // what is diff vs ensure_new_proposal?
    return ensure_events_internal<events::event_data_complete_proposal>(timeout, height, round);
  }

  bool ensure_precommit(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_vote>(timeout, height, round, p2p::signed_msg_type::Precommit);
  }

  bool ensure_prevote(int timeout, int64_t height, int32_t round) {
    return ensure_events_internal<events::event_data_vote>(timeout, height, round, p2p::signed_msg_type::Prevote);
  }

  std::string node_name;
  std::queue<events::message> msg_queue;
  int64_t height{-1};
  int32_t round{-1};
  p2p::round_step_type step;

  template<typename T>
  static int get_message_type_index() {
    events::tm_event_data tmp{T{}};
    return tmp.index();
  }

private:
  std::weak_ptr<events::event_bus> ev_bus;
  std::weak_ptr<consensus_state> cs_state_;
  events::event_bus::subscription handle;
  static std::map<std::string, p2p::round_step_type> step_map_;

  template<typename T>
  bool ensure_events_internal(int timeout, int64_t height = -1, int32_t round = -1,
    p2p::signed_msg_type vote_type = p2p::signed_msg_type::Unknown) {
    process_msg_result ret;

    std::atomic<bool> is_timeout = false;
    named_thread_pool thread_pool("status_monitor", 1);
    boost::asio::steady_timer timeout_ticker(thread_pool.get_executor());
    timeout_ticker.cancel();
    timeout_ticker.expires_from_now(std::chrono::seconds{timeout});
    timeout_ticker.async_wait([&is_timeout](const boost::system::error_code ec) {
      // if (ec.failed()) {}
      is_timeout = true;
    });

    while (ret = process_msg(), (is_timeout == false && ret.index < 0)) {
    }
    CHECKED_ELSE(is_timeout.load() == false) {
      return false;
    }
    CHECKED_ELSE(ret.index == get_message_type_index<T>()) {
      return false;
    }
    if (height >= 0) {
      CHECKED_ELSE(ret.height == height) {
        return false;
      }
    }
    if (round >= 0) {
      CHECKED_ELSE(ret.round == round) {
        return false;
      }
    }
    if (vote_type != p2p::signed_msg_type::Unknown) {
      CHECKED_ELSE(ret.vote_type == vote_type) {
        return false;
      }
    }
    return true;
  }
};

std::map<std::string, p2p::round_step_type> status_monitor::step_map_{
  {std::string{"NewHeight"}, p2p::round_step_type::NewHeight},
  {std::string{"NewRound"}, p2p::round_step_type::NewRound},
  {std::string{"Propose"}, p2p::round_step_type::Propose},
  {std::string{"Prevote"}, p2p::round_step_type::Prevote},
  {std::string{"PrevoteWait"}, p2p::round_step_type::PrevoteWait},
  {std::string{"Precommit"}, p2p::round_step_type::Precommit},
  {std::string{"PrecommitWait"}, p2p::round_step_type::PrecommitWait},
  {std::string{"Commit"}, p2p::round_step_type::Commit},
};

} // namespace noir::consensus
