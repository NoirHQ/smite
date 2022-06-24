// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/protobuf.h>
#include <noir/common/overloaded.h>
#include <noir/consensus/block_sync/block_pool.h>
#include <tendermint/blocksync/types.pb.h>

namespace noir::consensus::block_sync {

std::tuple<std::shared_ptr<block>, std::shared_ptr<block>> block_pool::peek_two_blocks() {
  std::scoped_lock g(mtx);
  std::shared_ptr<block> first(nullptr);
  std::shared_ptr<block> second(nullptr);
  auto r1 = requesters.find(height);
  if (r1 != requesters.end())
    first = r1->second->get_block();
  auto r2 = requesters.find(height + 1);
  if (r2 != requesters.end())
    second = r2->second->get_block();
  return {first, second};
}

void block_pool::pop_request() {
  std::scoped_lock g(mtx);

  auto r = requesters.find(height);
  if (r != requesters.end()) {
    r->second->on_stop(); // TODO: check if this is correct
    requesters.erase(r);
    height++;
    last_advance = get_time();

    // last_sync_rate will be updated every 100 blocks
    if ((height - start_height) % 100 == 0) {
      std::chrono::microseconds s(get_time() - last_hundred_block_timestamp); // TODO: check time precision
      auto new_sync_rate = 100 / std::chrono::duration_cast<std::chrono::seconds>(s).count(); // TODO: check
      if (last_sync_rate == 0)
        last_sync_rate = new_sync_rate;
      else
        last_sync_rate = 0.9 * last_sync_rate + 0.1 * new_sync_rate;
      last_hundred_block_timestamp = get_time();
    }
  } else {
    check(false, fmt::format("Panic: expected requester to pop, but got nothing at height={}", height));
  }
}

void block_pool::remove_timed_out_peers() {
  std::scoped_lock g(mtx);
  auto it = peers.begin();
  while (it != peers.end()) {
    std::shared_ptr<bp_peer>& peer = it->second;
    // Check if peer timed out // TODO : how to measure receive rate (apply limit rate?)
    // if (!peer->did_timeout && peer->num_pending > 0) {
    //   auto cur_rate = peer->recv_monitor
    // }
    it++; // advance to next item here, as remove_peer() below may delete current item
    if (peer->did_timeout)
      remove_peer(peer->id);
  }
}

void block_pool::remove_peer(std::string peer_id) {
  // Note: caller must have a lock on mtx
  for (auto const& [k, requester] : requesters) {
    if (requester->get_peer_id() == peer_id)
      requester->redo(peer_id);
  }

  auto it = peers.find(peer_id);
  if (it != peers.end()) {
    if (it->second->timeout != nullptr)
      it->second->timeout->cancel();
    if (it->second->height == max_peer_height)
      update_max_peer_height();
    peers.erase(it);
  }
}

void block_pool::update_max_peer_height() {
  int64_t max{0};
  for (auto const& [k, peer] : peers) {
    if (peer->height > max)
      max = peer->height;
  }
  max_peer_height = max;
}

std::shared_ptr<bp_peer> block_pool::pick_incr_available_peer(int64_t height_) {
  std::scoped_lock g(mtx);
  for (auto const& [k, peer] : peers) {
    if (peer->did_timeout) {
      remove_peer(peer->id);
      continue;
    }
    if (peer->num_pending >= max_pending_requests_per_peer)
      continue;
    if (height_ < peer->base || height_ > peer->height)
      continue;
    peer->incr_pending();
    return peer;
  }
  return {};
}

std::string block_pool::redo_request(int64_t height_) {
  std::scoped_lock g(mtx);
  auto it = requesters.find(height_);
  if (it == requesters.end())
    return "";
  auto peer_id = it->second->get_peer_id();
  if (peer_id != "")
    remove_peer(peer_id);
  return peer_id;
}

void block_pool::add_block(std::string peer_id_, std::shared_ptr<consensus::block> block_, int block_size) {
  std::scoped_lock g(mtx);
  auto requester = requesters.find(block_->header.height);
  if (requester == requesters.end()) {
    elog("peer sent us a block we didn't expect");
    auto diff = height - block_->header.height;
    if (diff < 0)
      diff *= -1;
    if (diff > max_diff_btn_curr_and_recv_block_height)
      send_error("peer sent us a block we didn't expect with a height too far ahead", peer_id_);
    return;
  }
  if (requester->second->set_block(block_, peer_id_)) {
    num_pending -= 1;
    auto peer = peers.find(peer_id_);
    if (peer != peers.end())
      peer->second->decr_pending(block_size);
  } else {
    std::string err("requester is different or block already exists");
    elog(err);
    send_error(err, peer_id_);
  }
}

void block_pool::set_peer_range(std::string peer_id_, int64_t base, int64_t height_) {
  std::scoped_lock g(mtx);
  auto peer = peers.find(peer_id_);
  if (peer != peers.end()) {
    peer->second->base = base;
    peer->second->height = height_;
  } else {
    auto new_peer = bp_peer::new_bp_peer(shared_from_this(), peer_id_, base, height_);
    peers[peer_id_] = new_peer;
  }
  if (height_ > max_peer_height)
    max_peer_height = height_;
}

void block_pool::make_next_requester() {
  std::unique_lock<std::mutex> g(mtx);
  int64_t next_height = height + requesters.size();
  if (next_height > max_peer_height)
    return;
  auto request = bp_requester::new_bp_requester(shared_from_this(), next_height);
  requesters[next_height] = request;
  num_pending += 1;
  g.unlock();
  request->on_start();
}

void block_pool::send_request(int64_t height_, std::string peer_id_) {
  if (!is_running)
    return;
  transmit_new_envelope(peer_id_, noir::consensus::block_request{height_});
}

void block_pool::send_error(std::string err, std::string peer_id_) {
  if (!is_running)
    return;
  app.get_method<plugin_interface::methods::send_error_to_peer>()(peer_id_, err);
}

void block_pool::transmit_new_envelope(const std::string& to, const p2p::bs_reactor_message& bs_msg, int priority) {
  dlog(fmt::format("[bs_reactor] send msg: to={}, type={}", to, bs_msg.index()));
  auto new_env = std::make_shared<p2p::envelope>();
  new_env->from = "";
  new_env->to = to;
  new_env->broadcast = false; // always false for block_sync
  new_env->id = p2p::BlockSync;

  // Use protobuf
  ::tendermint::blocksync::Message pb_msg;
  std::visit(
    overloaded{
      ///
      /*********************************************************************************************************/
      [&pb_msg](const consensus::block_request& msg) {
        auto req = pb_msg.mutable_block_request();
        req->set_height(msg.height);
      },
      [&pb_msg](const consensus::block_response& msg) {
        auto res = pb_msg.mutable_block_response();
        *res->mutable_block() = codec::protobuf::decode<::tendermint::types::Block>(msg.block_);
      },
      [&pb_msg](const consensus::status_request& msg) { auto req = pb_msg.mutable_status_request(); },
      [&pb_msg](const consensus::status_response& msg) {
        auto res = pb_msg.mutable_status_response();
        res->set_height(msg.height);
        res->set_base(msg.base);
      },
      [&pb_msg](const consensus::no_block_response& msg) {
        auto res = pb_msg.mutable_no_block_response();
        res->set_height(msg.height);
      },
    },
    bs_msg);
  new_env->message.resize(pb_msg.ByteSizeLong());
  pb_msg.SerializeToArray(new_env->message.data(), pb_msg.ByteSizeLong());

  xmt_mq_channel.publish(priority, new_env);
}

//------------------------------------------------------------------------
// bp_requester
//------------------------------------------------------------------------
bool bp_requester::set_block(std::shared_ptr<block> blk_, std::string peer_id_) {
  std::scoped_lock g(mtx);
  if (block_ != nullptr || peer_id != peer_id_)
    return false;
  block_ = blk_;

  // At the point, we have sent a request and received a response
  on_stop();
  return true;
}

void bp_requester::redo(std::string peer_id_) {
  if (!pool->is_running)
    return;
  if (peer_id_ == "")
    return;
  reset();
  request_routine(); // TODO: check if these sequence of actions are correct
}

void bp_requester::request_routine() {
  if (!pool->is_running)
    return;
  auto peer = pool->pick_incr_available_peer(height);
  if (peer) {
    std::unique_lock<std::mutex> lock(mtx);
    peer_id = peer->id;
    lock.unlock();

    // Send request
    pool->send_request(height, peer->id);
  } else {
    wlog(fmt::format("bp_requester failed: unable to find a peer, height={}", height));
  }
}

} // namespace noir::consensus::block_sync
