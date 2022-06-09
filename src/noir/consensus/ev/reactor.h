// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/plugin_interface.h>
#include <noir/common/thread_pool.h>
#include <noir/consensus/ev/evidence_pool.h>

namespace noir::consensus::ev {

struct reactor {

  appbase::application& app;
  std::shared_ptr<evidence_pool> pool{};
  std::unique_ptr<named_thread_pool> thread_pool{};

  std::mutex mtx;
  std::map<std::string, std::shared_ptr<bool>> peer_routines;

  // Receive an envelope from peers [via p2p]
  plugin_interface::incoming::channels::es_reactor_message_queue::channel_type::handle es_reactor_mq_subscription =
    app.get_channel<plugin_interface::incoming::channels::es_reactor_message_queue>().subscribe(
      std::bind(&reactor::process_peer_msg, this, std::placeholders::_1));

  // Receive peer_status update from p2p
  plugin_interface::channels::update_peer_status::channel_type::handle update_peer_status_subscription =
    app.get_channel<plugin_interface::channels::update_peer_status>().subscribe(
      std::bind(&reactor::process_peer_update, this, std::placeholders::_1));

  // Send an envelope to peers [via p2p]
  plugin_interface::egress::channels::transmit_message_queue::channel_type& xmt_mq_channel =
    app.get_channel<plugin_interface::egress::channels::transmit_message_queue>();

  reactor(appbase::application& app)
    : app(app), thread_pool(std::make_unique<named_thread_pool>("es_reactor_thread", 3)) {}

  static std::shared_ptr<reactor> new_reactor(
    appbase::application& new_app, const std::shared_ptr<evidence_pool>& new_pool) {
    auto ret = std::make_shared<reactor>(new_app);
    ret->pool = new_pool;
    return ret;
  }

  void process_peer_update(plugin_interface::peer_status_info_ptr info);

  Result<void> process_peer_msg(p2p::envelope_ptr info);

  void broadcast_evidence_loop(const std::string& peer_id, const std::shared_ptr<bool>& closer);
};

} // namespace noir::consensus::ev
