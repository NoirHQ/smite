// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/event_bus.h>
#include <noir/consensus/peer_state.h>
#include <noir/consensus/state.h>
#include <noir/consensus/store/store_test.h>
#include <noir/consensus/types.h>
#include <noir/consensus/types/events.h>

namespace noir::consensus {

struct consensus_reactor {
  appbase::application& app;

  std::shared_ptr<consensus_state> cs_state;

  std::shared_ptr<events::event_bus> event_bus_;

  std::map<std::string, std::shared_ptr<peer_state>> peers;
  bool wait_sync;

  std::mutex mtx;

  uint16_t thread_pool_size = 5;
  std::optional<named_thread_pool> thread_pool_gossip;
  std::optional<named_thread_pool> thread_pool_query_maj23;

  // Receive an event from consensus_state
  plugin_interface::egress::channels::event_switch_message_queue::channel_type::handle event_switch_mq_subscription =
    app.get_channel<plugin_interface::egress::channels::event_switch_message_queue>().subscribe(
      std::bind(&consensus_reactor::process_event, this, std::placeholders::_1));

  // Receive peer_status update from p2p
  plugin_interface::channels::update_peer_status::channel_type::handle update_peer_status_subscription =
    app.get_channel<plugin_interface::channels::update_peer_status>().subscribe(
      std::bind(&consensus_reactor::process_peer_update, this, std::placeholders::_1));

  // Receive an envelope from peers [via p2p]
  plugin_interface::incoming::channels::cs_reactor_message_queue::channel_type::handle cs_reactor_mq_subscription =
    app.get_channel<plugin_interface::incoming::channels::cs_reactor_message_queue>().subscribe(
      std::bind(&consensus_reactor::process_peer_msg, this, std::placeholders::_1));

  // Send an envelope to peers [via p2p]
  plugin_interface::egress::channels::transmit_message_queue::channel_type& xmt_mq_channel;

  // Send messages [block_part_message, vote_message] to consensus // TODO: check if this correct
  plugin_interface::channels::internal_message_queue::channel_type& internal_mq_channel =
    app.get_channel<plugin_interface::channels::internal_message_queue>();

  consensus_reactor(appbase::application& app,
    std::shared_ptr<consensus_state> new_cs_state,
    const std::shared_ptr<events::event_bus>& event_bus_,
    bool new_wait_sync)
    : app(app),
      cs_state(std::move(new_cs_state)),
      event_bus_(event_bus_),
      wait_sync(new_wait_sync),
      xmt_mq_channel(app.get_channel<plugin_interface::egress::channels::transmit_message_queue>()) {
    thread_pool_gossip.emplace("gossip", thread_pool_size);
    thread_pool_query_maj23.emplace("query_maj23", thread_pool_size);
  }

  static std::shared_ptr<consensus_reactor> new_consensus_reactor(appbase::application& app,
    std::shared_ptr<consensus_state>& new_cs_state,
    const std::shared_ptr<events::event_bus>& event_bus_,
    bool new_wait_sync) {
    auto consensus_reactor_ = std::make_shared<consensus_reactor>(app, new_cs_state, event_bus_, new_wait_sync);
    return consensus_reactor_;
  }

  void on_start() {
    ilog(fmt::format("starting cs_reactor... wait_sync={}", wait_sync));
    if (!wait_sync)
      cs_state->on_start();
  }

  void on_stop() {
    ilog("stopping cs_reactor...");
    for (auto const& peer : peers) {
      if (peer.second->is_running)
        peer.second->is_running = false;
    }
    thread_pool_gossip->stop();
    thread_pool_query_maj23->stop();
    cs_state->on_stop();
    ilog("stopped cs_reactor");
  }

  void switch_to_consensus(state& state_, bool skip_wal) {
    ilog("switched to consensus");

    if (state_.last_block_height > 0)
      cs_state->reconstruct_last_commit(state_);

    cs_state->update_to_state(state_);

    wait_sync = false;

    if (skip_wal)
      cs_state->do_wal_catchup = false;

    cs_state->on_start();

    event_bus_->publish_event_block_sync_status(events::event_data_block_sync_status{
      .complete = true,
      .height = cs_state->get_last_height(),
    });
  }

  void process_event(const plugin_interface::event_info_ptr& info) {
    switch (info->event_) {
    case EventNewRoundStep:
      broadcast_new_round_step_message(std::get<round_state>(info->message_));
      break;
    case EventValidBlock:
      broadcast_new_valid_block_message(std::get<round_state>(info->message_));
      break;
    case EventVote:
      broadcast_has_vote_message(std::get<p2p::vote_message>(info->message_));
      break;
    }
  }

  void process_peer_update(plugin_interface::peer_status_info_ptr info);

  void process_peer_msg(p2p::envelope_ptr info);

  void gossip_data_routine(std::shared_ptr<peer_state> ps);

  void gossip_data_for_catchup(const std::shared_ptr<round_state>& rs,
    const std::shared_ptr<peer_round_state>& prs,
    const std::shared_ptr<peer_state>& ps);

  void gossip_votes_routine(std::shared_ptr<peer_state> ps);

  bool gossip_votes_for_height(const std::shared_ptr<round_state>& rs,
    const std::shared_ptr<peer_round_state>& prs,
    const std::shared_ptr<peer_state>& ps);

  bool pick_send_vote(const std::shared_ptr<peer_state>& ps, const vote_set_reader& votes_);

  void query_maj23_routine(std::shared_ptr<peer_state> ps);

  std::shared_ptr<peer_state> get_peer_state(std::string peer_id) {
    std::scoped_lock g(mtx);
    auto it = peers.find(peer_id);
    return it == peers.end() ? nullptr : it->second;
  }

  void broadcast_new_round_step_message(const round_state& rs) {
    auto msg = make_round_step_message(rs);
    transmit_new_envelope("", "", msg, true);
  }

  void broadcast_new_valid_block_message(const round_state& rs) {
    auto msg = p2p::new_valid_block_message{rs.height, rs.round,
      rs.proposal_block_parts->header() /* TODO: check if this is correct*/, rs.proposal_block_parts->get_bit_array(),
      rs.step == p2p::round_step_type::Commit};
    transmit_new_envelope("", "", msg, true);
  }

  void broadcast_has_vote_message(const p2p::vote_message& vote_) {
    auto msg = p2p::has_vote_message{vote_.height, vote_.round, vote_.type, vote_.validator_index};
    transmit_new_envelope("", "", msg, true);
  }

  p2p::new_round_step_message make_round_step_message(const round_state& rs) {
    return p2p::new_round_step_message{
      rs.height, rs.round, rs.step, rs.start_time /* TODO: find elapsed seconds*/, rs.last_commit->round};
  }

  void send_new_round_step_message(std::string peer_id);

  void transmit_new_envelope(std::string from,
    std::string to,
    const p2p::cs_reactor_message& msg,
    bool broadcast = false,
    int priority = appbase::priority::medium);
};

} // namespace noir::consensus
