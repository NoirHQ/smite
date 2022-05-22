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
  std::shared_ptr<evidence_pool> pool;
  std::unique_ptr<named_thread_pool> thread_pool;

  // Receive an envelope from peers [via p2p]
  plugin_interface::incoming::channels::bs_reactor_message_queue::channel_type::handle bs_reactor_mq_subscription =
    app.get_channel<plugin_interface::incoming::channels::es_reactor_message_queue>().subscribe(
      std::bind(&reactor::process_peer_msg, this, std::placeholders::_1));

  // Receive peer_status update from p2p
  plugin_interface::channels::update_peer_status::channel_type::handle update_peer_status_subscription =
    app.get_channel<plugin_interface::channels::update_peer_status>().subscribe(
      std::bind(&reactor::process_peer_update, this, std::placeholders::_1));

  reactor(appbase::application& app)
    : app(app), thread_pool(std::make_unique<named_thread_pool>("bs_reactor_thread", 3)) {}

  void process_peer_msg(p2p::envelope_ptr info) {}

  void process_peer_update(plugin_interface::peer_status_info_ptr info) {
    dlog(fmt::format(
      "[es_reactor] peer update: peer_id={}, status={}", info->peer_id, p2p::peer_status_to_str(info->status)));
    switch (info->status) {
    case p2p::peer_status::up: {
      break;
    }
    case p2p::peer_status::down: {
      break;
    }
    default:
      break;
    }
  }
};

} // namespace noir::consensus::ev
