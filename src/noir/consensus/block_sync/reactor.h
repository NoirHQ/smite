// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/block_executor.h>
#include <noir/consensus/block_sync/block_pool.h>
#include <noir/consensus/consensus_reactor.h>
#include <noir/consensus/state.h>

namespace noir::consensus::block_sync {

constexpr auto try_sync_interval{std::chrono::milliseconds(10)};
constexpr auto status_update_interval{std::chrono::seconds(10)};
constexpr auto switch_to_consensus_interval{std::chrono::seconds(1)};
constexpr auto sync_timeout{std::chrono::seconds(60)};

struct reactor {
  reactor(appbase::application& app)
    : app(app), thread_pool(std::make_unique<named_thread_pool>("bs_reactor_thread", 1)) {}

  appbase::application& app;

  consensus::state initial_state;
  consensus::state latest_state;
  uint64_t blocks_synced{};

  std::shared_ptr<consensus::block_executor> block_exec;
  std::shared_ptr<consensus::block_store> store;
  std::shared_ptr<block_pool> pool;

  std::atomic_bool block_sync;

  p2p::tstamp sync_start_time;

  std::function<void(state&, bool)> callback_switch_to_cs_sync{};

  // Receive an envelope from peers [via p2p]
  plugin_interface::incoming::channels::bs_reactor_message_queue::channel_type::handle bs_reactor_mq_subscription =
    app.get_channel<plugin_interface::incoming::channels::bs_reactor_message_queue>().subscribe(
      std::bind(&reactor::process_peer_msg, this, std::placeholders::_1));

  // Receive peer_status update from p2p
  plugin_interface::channels::update_peer_status::channel_type::handle update_peer_status_subscription =
    app.get_channel<plugin_interface::channels::update_peer_status>().subscribe(
      std::bind(&reactor::process_peer_update, this, std::placeholders::_1));

  std::unique_ptr<named_thread_pool> thread_pool;

  static std::shared_ptr<reactor> new_reactor(appbase::application& app, state& state_,
    const std::shared_ptr<block_executor>& block_exec_, const std::shared_ptr<block_store>& new_block_store,
    bool block_sync_) {

    if (state_.last_block_height != new_block_store->height()) {
      elog("block_sync_reactor: state_ and store height mismatch");
      return {};
    }

    auto start_height = new_block_store->height() + 1;
    if (start_height == 1)
      start_height = state_.initial_height;

    auto reactor_ = std::make_shared<reactor>(app);
    reactor_->initial_state = state_;
    reactor_->block_exec = block_exec_;
    reactor_->store = new_block_store;
    reactor_->pool = block_pool::new_block_pool(app, start_height);
    reactor_->block_sync = block_sync_;
    reactor_->sync_start_time = 0;

    return reactor_;
  }

  void on_start() {
    if (block_sync.load()) {
      pool->on_start();
      pool_routine(false);
    }
  }

  void on_stop() {
    ilog("stopping bs_reactor...");
    bool expected = true;
    if (block_sync.compare_exchange_strong(expected, false)) {
      pool->on_stop();
    }
    thread_pool->stop();
    ilog("stopped bs_reactor");
  }

  // TODO : check this function is safe
  void switch_to_block_sync(state& state_) {
    block_sync.store(true);
    initial_state = state_;
    pool->height = state_.last_block_height + 1;

    pool->on_start();
    sync_start_time = get_time();
    pool_routine(true);
  }

  void set_callback_switch_to_cs_sync(std::function<void(state&, bool)> cb) {
    callback_switch_to_cs_sync = std::move(cb);
  }

  void process_peer_update(plugin_interface::peer_status_info_ptr info);
  void process_peer_msg(p2p::envelope_ptr info);

  /// \brief periodically request for status from all peers
  void request_routine();

  /// \brief handles messages from block_pool
  void pool_routine(bool state_synced);

  void try_sync_ticker();
  void switch_to_consensus_ticker();

  void respond_to_peer(std::shared_ptr<consensus::block_request> msg, const std::string& peer_id);

  int64_t get_max_peer_block_height() const {
    return pool->max_peer_height;
  }

  boost::asio::steady_timer::duration get_total_synced_time() {
    if (block_sync.load() != true || sync_start_time == 0)
      return std::chrono::seconds(0);
    std::chrono::microseconds s(get_time() - sync_start_time);
    return std::chrono::duration_cast<std::chrono::seconds>(s);
  }

  boost::asio::steady_timer::duration get_remaining_sync_time() {
    if (block_sync.load() != true)
      return std::chrono::seconds(0);
    // auto target_syncs = pool->target_sync_block(); // Note: optional to implement; it's not used for now
    return std::chrono::seconds(10);
  }
};

} // namespace noir::consensus::block_sync
