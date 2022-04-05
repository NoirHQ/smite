// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/plugin_interface.h>
#include <noir/consensus/common_test.h>
#include <noir/consensus/node.h>
#include <noir/p2p/types.h>
#include <appbase/application.hpp>

using namespace noir;
using namespace noir::consensus;

namespace test_detail {
using namespace noir::consensus::events;

struct channel_stub {
public:
  plugin_interface::channels::update_peer_status::channel_type& update_peer_status_channel;
  plugin_interface::egress::channels::transmit_message_queue::channel_type::handle xmt_mq_subscription;

  plugin_interface::incoming::channels::bs_reactor_message_queue::channel_type& bs_reactor_mq_channel;
  plugin_interface::incoming::channels::cs_reactor_message_queue::channel_type& cs_reactor_mq_channel;

  channel_stub() = delete;
  explicit channel_stub(appbase::application& app)
    : update_peer_status_channel(app.get_channel<plugin_interface::channels::update_peer_status>()),
      bs_reactor_mq_channel(app.get_channel<plugin_interface::incoming::channels::bs_reactor_message_queue>()),
      cs_reactor_mq_channel(app.get_channel<plugin_interface::incoming::channels::cs_reactor_message_queue>()) {}
};

std::atomic_bool is_running = false;

class test_node {
public:
  test_node() = delete;
  test_node(test_node&&) = default;
  test_node(const test_node&) = delete;
  test_node& operator=(const test_node&&) = delete;
  test_node(int num, std::unique_ptr<appbase::application> app, const std::shared_ptr<genesis_doc>& gen_doc,
    const std::shared_ptr<priv_validator>& priv_val)
    : app_(std::move(app)), channel_stub_(std::make_shared<channel_stub>(*app_)), node_number_(num) {
    node_name_ = "node_" + to_string(node_number_);

    channel_stub_->xmt_mq_subscription =
      app_->get_channel<plugin_interface::egress::channels::transmit_message_queue>().subscribe(
        [this](auto&& arg) { route_message(std::forward<decltype(arg)>(arg)); });

    auto cfg = std::make_shared<config>(config::get_default());
    cfg->base.chain_id = "test_chain";
    cfg->base.mode = Validator;
    cfg->base.node_key = node_name_;
    cfg->base.root_dir = "/tmp/noir_test/" + node_name_;
    cfg->consensus.root_dir = cfg->base.root_dir;
    cfg->priv_validator.root_dir = cfg->base.root_dir;

    std::filesystem::remove_all(cfg->consensus.root_dir); // delete exist node directory
    std::filesystem::create_directories(cfg->consensus.root_dir);
    auto db_dir = std::filesystem::path{cfg->consensus.root_dir} / "db";
    auto session = make_session(true, db_dir);

    node_ = node::make_node(*app_, cfg, priv_val, node_key::gen_node_key(), gen_doc, session);

    node_->bs_reactor->set_callback_switch_to_cs_sync([cs_reactor = node_->cs_reactor](auto&& arg1, auto&& arg2) {
      cs_reactor->switch_to_consensus(std::forward<decltype(arg1)>(arg1), std::forward<decltype(arg2)>(arg2));
    });
    monitor = status_monitor(node_name_, node_->event_bus_, node_->cs_reactor->cs_state);
    monitor.subscribe_filtered_msg([](const events::message&) { return false; }); // event is not used yet

    thread_ = std::make_unique<noir::named_thread_pool>("test_thread", 2);
  }

  ~test_node() {
    app_->quit();
    thread_->stop();
  }

  void start() {
    noir::async_thread_pool(thread_->get_executor(), [this]() {
      app_->register_plugin<test_plugin>();
      app_->initialize<test_plugin>();
      app_->startup();
      app_->exec();
    });
    noir::async_thread_pool(thread_->get_executor(), [this]() {
      while (!is_running.load(std::memory_order_acquire)) {
        usleep(1000);
      }
      node_->on_start();
    });
  }

  void stop() {
    node_->on_stop();
  }

  void peer_update(const std::shared_ptr<test_node>& other_node) {
    peers_.push_back(other_node);
    channel_stub_->update_peer_status_channel.publish(appbase::priority::medium,
      std::make_shared<plugin_interface::peer_status_info>(
        plugin_interface::peer_status_info{other_node->node_name(), p2p::peer_status::up}));
  }

  void handle_message(const p2p::envelope_ptr& env) {
    ilog(fmt::format("receive msg. from={}, to={}, id={}, broadcast={}", env->from, env->to, env->id, env->broadcast));
    switch (env->id) {
    case p2p::Consensus: {
      channel_stub_->cs_reactor_mq_channel.publish(appbase::priority::medium, env);
      break;
    }

    case p2p::BlockSync: {
      channel_stub_->bs_reactor_mq_channel.publish(appbase::priority::medium, env);
      break;
    }

    case p2p::PeerError:
    default:
      // TODO : handling error case?
      break;
    }
  }

  void route_message(const p2p::envelope_ptr& env) {
    env->from = node_name_;
    if (env->broadcast) {
      for (auto& n : peers_) {
        auto node = n.lock();
        if (node) {
          node->handle_message(env);
        }
      }
    } else {
      for (auto& n : peers_) {
        auto node = n.lock();
        if (node && (node->node_name() == env->to)) {
          node->handle_message(env);
          return;
        }
      }
    }
  }

  std::string node_name() const {
    return node_name_;
  }

  status_monitor monitor;

private:
  const int node_number_;
  std::string node_name_;
  std::unique_ptr<appbase::application> app_;
  std::unique_ptr<node> node_;
  std::shared_ptr<channel_stub> channel_stub_;
  std::unique_ptr<noir::named_thread_pool> thread_;
  std::vector<std::weak_ptr<test_node>> peers_;
};

std::vector<std::shared_ptr<test_node>> make_node_set(int count) {
  auto cfg = config::get_default();
  cfg.base.chain_id = "test_chain";
  auto [gen_doc, priv_vals] = rand_genesis_doc(cfg, count, false, 100 / count);
  auto gen_doc_ptr = std::make_shared<genesis_doc>(gen_doc);

  std::vector<std::shared_ptr<test_node>> test_nodes;
  test_nodes.reserve(count);
  for (int i = 0; i < count; i++) {
    test_nodes.emplace_back(
      std::make_shared<test_node>(i, std::make_unique<appbase::application>(), gen_doc_ptr, priv_vals[i]));
  }

  return test_nodes;
}

void ice_breaking(const std::vector<std::shared_ptr<test_node>>& test_nodes) {
  for (auto& test_node : test_nodes) {
    for (auto& n : test_nodes) {
      if (test_node == n)
        continue;
      n->peer_update(test_node);
    }
  }
}

} // namespace test_detail

TEST_CASE("node: multiple validator test") {
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
  int node_count = 2;
  int test_time = 100; // seconds
  int sleep_interval = 3; // seconds

  auto test_nodes = test_detail::make_node_set(node_count);

  for (auto& test_node : test_nodes) {
    test_node->start();
  }
  test_detail::is_running.store(true, std::memory_order_seq_cst);

  test_detail::ice_breaking(test_nodes);

  for (auto i = 0; i < test_time; i += sleep_interval) {
    sleep(sleep_interval);
    std::cout << "elapsed_time=" << i << std::endl;
    for (auto& test_node : test_nodes) {
      status_monitor::process_msg_result ret;
      while (ret = test_node->monitor.process_msg(), ret.index >= 0) {
        std::cout << ret.to_string() << std::endl;
      }
    }
  }

  test_detail::is_running.store(false, std::memory_order_relaxed);

  for (auto& test_node : test_nodes) {
    test_node->stop();
  }
}
