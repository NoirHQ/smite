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

struct channel_stub {
public:
  // common channels
  plugin_interface::channels::update_peer_status::channel_type& update_peer_status_channel;

  plugin_interface::egress::channels::transmit_message_queue::channel_type& xmt_mq_channel;

  // block_sync_reactor channels
  plugin_interface::incoming::channels::bs_reactor_message_queue::channel_type& bs_reactor_mq_channel;

  // consensus_reactor channels
  plugin_interface::egress::channels::event_switch_message_queue::channel_type& event_switch_mq_channel;

  plugin_interface::incoming::channels::cs_reactor_message_queue::channel_type& cs_reactor_mq_channel;

  plugin_interface::channels::internal_message_queue::channel_type& internal_mq_channel;

  channel_stub()
    : update_peer_status_channel(appbase::app().get_channel<plugin_interface::channels::update_peer_status>()),
      xmt_mq_channel(appbase::app().get_channel<plugin_interface::egress::channels::transmit_message_queue>()),
      bs_reactor_mq_channel(
        appbase::app().get_channel<plugin_interface::incoming::channels::bs_reactor_message_queue>()),
      event_switch_mq_channel(
        appbase::app().get_channel<plugin_interface::egress::channels::event_switch_message_queue>()),
      cs_reactor_mq_channel(
        appbase::app().get_channel<plugin_interface::incoming::channels::cs_reactor_message_queue>()),
      internal_mq_channel(appbase::app().get_channel<plugin_interface::channels::internal_message_queue>()) {}
};

std::unique_ptr<node> new_test_node(
  int num, std::shared_ptr<genesis_doc> gen_doc, std::shared_ptr<priv_validator> priv_val) {
  auto cfg = std::make_shared<config>(config::get_default());
  cfg->base.chain_id = "test_chain";
  cfg->base.mode = Validator;
  cfg->base.root_dir = "/tmp/noir_test/node_" + to_string(num);
  cfg->consensus.root_dir = cfg->base.root_dir;
  cfg->priv_validator.root_dir = cfg->base.root_dir;

  auto db_dir = std::filesystem::path{cfg->consensus.root_dir} / "db";
  auto session =
    std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session(false, db_dir));

  auto node = node::make_node(cfg, priv_val, node_key::gen_node_key(), gen_doc, session);

  node->bs_reactor->set_callback_switch_to_cs_sync([ObjectPtr = node->cs_reactor](auto&& PH1, auto&& PH2) {
    ObjectPtr->switch_to_consensus(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
  });

  return node;
}

std::vector<std::unique_ptr<node>> make_node_set(int count) {
  auto cfg = config::get_default();
  cfg.base.chain_id = "test_chain";
  auto [gen_doc, priv_vals] = rand_genesis_doc(cfg, count, false, 100 / count);
  auto gen_doc_ptr = std::make_shared<genesis_doc>(gen_doc);
  std::vector<std::unique_ptr<node>> nodes;
  nodes.reserve(count);
  for (int i = 0; i < count; i++) {
    nodes.push_back(new_test_node(i, gen_doc_ptr, priv_vals[i]));
  }
  return nodes;
}

} // namespace test_detail

TEST_CASE("multiple validator test") {
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
  auto nodes = test_detail::make_node_set(1);

  for (auto& node : nodes) {
    node->on_start();
  }

  for (auto& node : nodes) {
    node->on_stop();
  }
}
