// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/plugin_interface.h>
#include <noir/common/thread_pool.h>
#include <noir/consensus/block.h>

#include <utility>

namespace noir::consensus::block_sync {

constexpr auto request_interval{std::chrono::milliseconds(2)};
constexpr int max_total_requesters{30}; // default = 600
constexpr int max_peer_err_buffer{1000};
constexpr int max_pending_requests{max_total_requesters};
constexpr int max_pending_requests_per_peer{20};
constexpr int min_recv_rate{7680};
constexpr int max_diff_btn_curr_and_recv_block_height{100};

constexpr auto peer_timeout{std::chrono::seconds(15)};

struct bp_requester;
struct bp_peer;

/// \brief keeps track of block sync peers, block requests and block responses
struct block_pool : std::enable_shared_from_this<block_pool> {
  p2p::tstamp last_advance;

  std::mutex mtx;
  std::map<int64_t, std::shared_ptr<bp_requester>> requesters;
  int64_t height;
  std::map<std::string, std::shared_ptr<bp_peer>> peers;
  int64_t max_peer_height;

  int32_t num_pending;
  int64_t start_height;
  p2p::tstamp last_hundred_block_timestamp;
  double last_sync_rate;

  bool is_running{}; ///< used to end active jobs on_stop
  uint16_t thread_pool_size = 15;
  std::optional<named_thread_pool> thread_pool;
  std::shared_ptr<boost::asio::io_context::strand> strand;

  // Send an envelope to peers [via p2p]
  plugin_interface::egress::channels::transmit_message_queue::channel_type& xmt_mq_channel =
    appbase::app().get_channel<plugin_interface::egress::channels::transmit_message_queue>();

  block_pool() {
    thread_pool.emplace("cs_reactor", thread_pool_size);
    strand = std::make_shared<boost::asio::io_context::strand>(thread_pool->get_executor());
  }

  static std::shared_ptr<block_pool> new_block_pool(int64_t start) {
    auto bp = std::make_shared<block_pool>();
    bp->height = start;
    bp->start_height = start;
    bp->num_pending = 0;
    bp->last_sync_rate = 0;
    return bp;
  }

  void on_start() {
    last_advance = get_time();
    last_hundred_block_timestamp = last_advance;
    is_running = true;
    make_requester_routine();
  }
  void on_stop() {
    // TODO: any other clean up to do?
    is_running = false;
  }

  void make_requester_routine() {
    strand->post([this]() {
      if (!is_running)
        return;

      auto [h, num_pending_, num_requesters_] = get_status();
      if (num_pending_ >= max_pending_requests) {
        std::this_thread::sleep_for(request_interval);
        remove_timed_out_peers();
      } else if (num_requesters_ >= max_total_requesters) {
        std::this_thread::sleep_for(request_interval);
        remove_timed_out_peers();
      } else {
        make_next_requester();
      }
      make_requester_routine();
    });
  }

  std::tuple<int64_t, int32_t, int> get_status() {
    std::lock_guard<std::mutex> g(mtx);
    return {height, num_pending, requesters.size()};
  }

  bool is_caught_up() {
    std::lock_guard<std::mutex> g(mtx);
    if (peers.empty())
      return false;
    return height >= (max_peer_height - 1);
  }

  std::vector<std::string> get_peer_ids() {
    std::lock_guard<std::mutex> g(mtx);
    std::vector<std::string> ret;
    for (const auto& pair : peers)
      ret.push_back(pair.first);
    return ret;
  }

  std::tuple<std::shared_ptr<block>, std::shared_ptr<block>> peek_two_blocks();

  void pop_request();

  void remove_timed_out_peers();
  void remove_peer(std::string peer_id);
  void update_max_peer_height();
  std::shared_ptr<bp_peer> pick_incr_available_peer(int64_t height);

  std::string redo_request(int64_t height);

  void add_block(std::string peer_id, std::shared_ptr<consensus::block> block_, int block_size);

  void set_peer_range(std::string peer_id_, int64_t base, int64_t height_);

  void make_next_requester();

  void send_request(int64_t height, std::string peer_id);

  void send_error(std::string err, std::string peer_id);

  void transmit_new_envelope(const std::string& from, const std::string& to, const p2p::bs_reactor_message& msg,
    bool broadcast = false, int priority = appbase::priority::medium);
};

/// \brief periodically requests until block is received for a height
/// once initialized, height never changes; peer_id may change as we may send a request to any peers
struct bp_requester {
  std::shared_ptr<block_pool> pool;
  int64_t height;

  std::mutex mtx;
  std::string peer_id;
  std::shared_ptr<consensus::block> block_;

  bool is_running{}; ///< used to end active jobs on_stop
  std::shared_ptr<boost::asio::io_context::strand> strand;

  static std::shared_ptr<bp_requester> new_bp_requester(std::shared_ptr<block_pool> new_pool_, int64_t height_) {
    auto bpr = std::make_shared<bp_requester>();
    bpr->pool = std::move(new_pool_);
    bpr->height = height_;
    bpr->peer_id = "";
    bpr->block_ = nullptr;
    assert(bpr->pool);
    bpr->strand = std::make_shared<boost::asio::io_context::strand>(bpr->pool->thread_pool->get_executor());
    return bpr;
  }

  void on_start() {
    is_running = true;
    request_routine();
  }

  void on_stop() {
    is_running = false; // TODO: is this enough?
  }

  bool set_block(std::shared_ptr<block> blk_, std::string peer_id_);

  std::shared_ptr<block> get_block() {
    std::lock_guard<std::mutex> g(mtx);
    return block_;
  }

  std::string get_peer_id() {
    std::lock_guard<std::mutex> g(mtx);
    return peer_id;
  }

  void reset() {
    std::lock_guard<std::mutex> g(mtx);
    if (block_ != nullptr)
      pool->num_pending += 1;
    peer_id = "";
    block_ = nullptr;
  }

  void redo(std::string peer_id);

  void request_routine();
};

/// \brief keeps monitoring a peer
/// remove a peer from a list of peers when it doesn't send us anything for a while
struct bp_peer {
  bool did_timeout;
  int32_t num_pending;
  int64_t height;
  int64_t base;
  std::shared_ptr<block_pool> pool;
  std::string id;
  // recv_monitor // TODO: maybe not needed?

  std::shared_ptr<boost::asio::steady_timer> timeout;

  static std::shared_ptr<bp_peer> new_bp_peer(std::shared_ptr<block_pool> pool_, std::string peer_id, int64_t base_,
    int64_t height_, boost::asio::io_context& ioc) {
    auto peer = std::make_shared<bp_peer>();
    peer->pool = pool_;
    peer->id = peer_id;
    peer->base = base_;
    peer->height = height_;
    peer->timeout.reset(new boost::asio::steady_timer(ioc));
    peer->reset_timeout();
    peer->timeout->async_wait([peer](boost::system::error_code ec) {
      if (ec)
        return;
      peer->on_timeout();
    });
    return peer;
  }

  void on_timeout() {
    std::lock_guard<std::mutex> g(pool->mtx);
    std::string err("peer did not send us anything for a while");
    pool->send_error(err, id);
    elog("send_timeout: reason=${reason} sender=${sender}", ("reason", err)("sender", id));
    did_timeout = true;
  }

  void incr_pending() {
    if (num_pending == 0) {
      // reset_monitor(); // TODO: maybe not needed?
      reset_timeout();
    }
    num_pending++;
  }

  void decr_pending(int recv_size) {
    num_pending--;
    if (num_pending == 0) {
      if (timeout)
        timeout->cancel();
      on_timeout(); // manually call here, as cancelling timeout does not invoke it
    } else {
      // recv_monitor.update(recv_size); // TODO: maybe not needed?
      reset_timeout();
    }
  }

  void reset_timeout() {
    if (timeout) {
      timeout->expires_from_now(peer_timeout);
    }
  }
};

} // namespace noir::consensus::block_sync
