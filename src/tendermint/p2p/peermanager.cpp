// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/defer.h>
#include <tendermint/p2p/peermanager.h>

namespace tendermint::p2p {
using namespace noir;
using namespace std::chrono;

auto PeerManagerOptions::is_persistent(const NodeId& id) -> bool {
  if (persistent_peers_lookup.empty()) {
    throw std::runtime_error("is_persistent() called before optimize()");
  }
  return persistent_peers_lookup[id];
}

auto PeerInfo::score() -> PeerScore {
  if (fixed_score > 0) {
    return fixed_score;
  }
  if (persistent) {
    return peer_score_persistent;
  }

  auto score = mutable_score;
  if (score > int64_t(max_peer_score_not_persistent)) {
    score = int64_t(max_peer_score_not_persistent);
  }

  for (auto& [_, addr] : address_info) {
    score -= int64_t(addr->dial_failures);
  }

  if (score < std::numeric_limits<int16_t>::min()) {
    score = std::numeric_limits<int16_t>::min();
  }
  return PeerScore(score);
}

auto PeerInfo::validate() -> noir::Result<void> {
  if (id.empty()) {
    return Error("no peer ID");
  }
  return success();
}

auto PeerStore::get(const NodeId& peer_id) -> std::tuple<std::shared_ptr<PeerInfo>, bool> {
  auto ok = peers.contains(peer_id);
  return ok ? std::make_tuple(std::make_shared<PeerInfo>(*peers[peer_id]), ok) : std::make_tuple(nullptr, ok);
}

auto PeerStore::set(const std::shared_ptr<PeerInfo>& peer) -> noir::Result<void> {
  auto val_res = peer->validate();
  if (val_res) {
    return val_res.error();
  }
  auto copied_peer = std::make_shared<PeerInfo>(*peer);

  // TODO: DB
  // FIXME: We may want to optimize this by avoiding saving to the database
  // if there haven't been any changes to persisted fields.
  // bz, err := peer.ToProto().Marshal()
  // if err != nil {
  //   return err
  // }
  // if err = s.db.Set(keyPeerInfo(peer.ID), bz); err != nil {
  //   return err
  // }

  auto current_ok = peers.contains(copied_peer->id);
  auto current = peers[copied_peer->id];
  if (!current_ok || current->score() != copied_peer->score()) {
    peers[peer->id] = copied_peer;
    _ranked.clear();
  }
  for (auto& [addr, _]: copied_peer->address_info){
    index[addr] = copied_peer->id;
  }
  return success();
}

auto PeerStore::ranked() -> std::vector<std::shared_ptr<PeerInfo>> {
  if (!_ranked.empty()) {
    return _ranked;
  }
  for (auto& [_, peer] : peers) {
    _ranked.push_back(peer);
  }
  std::sort(_ranked.begin(), _ranked.end(), [](std::shared_ptr<PeerInfo>& p1, std::shared_ptr<PeerInfo> p2) -> bool {
    return p1->score() > p2->score();
  });
  return _ranked;
}

auto PeerStore::resolve(const std::shared_ptr<NodeAddress>& addr) -> std::tuple<NodeId, bool> {
  return std::make_tuple(index[addr], index.contains(addr));
}

auto PeerManager::has_max_peer_capacity() -> bool {
  std::scoped_lock lock(mtx);
  return connected.size() > options.max_connected;
}

auto PeerManager::errored(const NodeId& peer_id, Error& error) -> asio::awaitable<void> {
  std::scoped_lock _(mtx);

  if (is_connected(peer_id)) {
    evict[peer_id] = true;
  }

  co_await evict_waker.wake();
}

void PeerManager::process_peer_event(Chan<std::monostate>& done, const std::shared_ptr<PeerUpdate>& pu) {
  std::scoped_lock _(mtx);
  auto ok = store.peers.contains(pu->node_id);
  if (!ok) {
    store.peers[pu->node_id] = std::make_shared<PeerInfo>(pu->node_id);
  }
  if (pu->status == peer_status_bad) {
    if (store.peers[pu->node_id]->mutable_score == std::numeric_limits<int16_t>::min()) {
      return;
    }
    store.peers[pu->node_id]->mutable_score--;
  } else if (pu->status == peer_status_good) {
    if (store.peers[pu->node_id]->mutable_score == std::numeric_limits<int16_t>::max()) {
      return;
    }
    store.peers[pu->node_id]->mutable_score++;
  }
}

auto PeerManager::accepted(const NodeId& peer_id) -> asio::awaitable<Result<void>> {
  std::scoped_lock _(mtx);

  if (peer_id == self_id) {
    co_return Error::format("rejecting connection from self ({})", peer_id);
  }
  if (is_connected(peer_id)) {
    co_return Error::format("peer {} is already connected", peer_id);
  }
  if (options.max_connected > 0 && connected.size() >= options.max_connected + options.max_connected_upgrade) {
    co_return Error("already connected to maximum number of peers");
  }

  auto [peer, ok] = store.get(peer_id);
  if (!ok) {
    peer = new_peer_info(peer_id);
  }
  for (auto& [_, addr] : peer->address_info) {
    addr->dial_failures = 0;
  }

  NodeId upgrade_from_peer;
  if (options.max_connected > 0 && connected.size() >= options.max_connected) {
    upgrade_from_peer = find_upgrade_candidate(peer->id, peer->score());
    if (upgrade_from_peer.empty()) {
      co_return Error("already connected to maximum number of peers");
    }
  }
  peer->inactive = false;
  peer->last_connected = system_clock::to_time_t(system_clock::now());
  auto set_res = store.set(peer);
  if (set_res.has_error()) {
    co_return set_res.error();
  }

  connected[peer_id] = PeerConnectionDirection::peer_connection_incoming;
  if (!upgrade_from_peer.empty()) {
    evict[upgrade_from_peer] = true;
  }
  co_await evict_waker.wake();
  co_return success();
}

auto PeerManager::ready(Chan<std::monostate>& done, const NodeId& peer_id, ChannelIdSet& channels)
  -> asio::awaitable<void> {
  std::scoped_lock _(mtx);

  if (is_connected(peer_id)) {
    _ready[peer_id] = true;
    co_await broadcast(done, std::make_shared<PeerUpdate>(peer_id, peer_status_up, channels));
  }
}

auto PeerManager::dial_next(Chan<std::monostate>& done) -> asio::awaitable<Result<std::shared_ptr<NodeAddress>>> {
  for (;;) {
    auto address = try_dial_next();
    if (address) {
      co_return address;
    }

    auto waker_res = co_await (
      dial_waker.wake_ch.async_receive(asio::use_awaitable) || done.async_receive(as_result(asio::use_awaitable)));
    if (waker_res.index() == 0) {
    } else if (waker_res.index() == 1) {
      co_return std::get<1>(waker_res).error();
    } else {
      co_return err_unreachable;
    }
  }
}

auto PeerManager::evict_next(Chan<std::monostate>& done) -> boost::asio::awaitable<Result<NodeId>> {
  for (;;) {
    auto evict_res = try_evict_next();
    if (evict_res.has_error()) {
      co_return evict_res.error();
    } else if (!evict_res.value().empty()) {
      co_return evict_res.value();
    }

    auto waker_res = co_await (
      evict_waker.wake_ch.async_receive(asio::use_awaitable) || done.async_receive(as_result(asio::use_awaitable)));
    if (waker_res.index() == 0) {
    } else if (waker_res.index() == 1) {
      co_return std::get<1>(waker_res).error();
    } else {
      co_return err_unreachable;
    }
  }
}

auto PeerManager::try_evict_next() -> Result<NodeId> {
  std::scoped_lock _(mtx);

  for (auto pos = evict.begin(); pos != evict.end();) {
    auto peer_id = pos->first;
    evict.erase(pos++);
    if (is_connected(peer_id) && !evicting.contains(peer_id)) {
      evicting[peer_id] = true;
      return peer_id;
    }
  }

  if (options.max_connected == 0 || connected.size() - evicting.size() <= options.max_connected) {
    return "";
  }

  auto ranked = store.ranked();
  for (auto i = ranked.size() - 1; i >= 0; i--) {
    auto peer = ranked.at(i);
    if (is_connected(peer->id) && !evicting.contains(peer->id)) {
      evicting[peer->id] = true;
      return peer->id;
    }
  }
  return "";
}

auto PeerManager::inactivate(const NodeId& peer_id) -> Result<void> {
  std::scoped_lock _(mtx);
  auto ok = store.peers.contains(peer_id);
  if (!ok) {
    return success();
  }

  auto peer = store.peers[peer_id];
  peer->inactive = true;
  return store.set(peer);
}

auto PeerManager::dial_failed(Chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
  -> asio::awaitable<Result<void>> {
  std::scoped_lock _(mtx);
  dialing.erase(address->node_id);

  for (auto pos = upgrading.begin(); pos != upgrading.end();) {
    if (pos->second == address->node_id) {
      upgrading.erase(pos++);
    } else {
      ++pos;
    }
  }

  auto [peer, ok] = store.get(address->node_id);
  if (!ok) {
    co_return success();
  }
  auto addr_ok = peer->address_info.contains(address);
  if (!addr_ok) {
    co_return success();
  }
  auto addr_info = peer->address_info[address];
  addr_info->last_dial_failure = system_clock::to_time_t(system_clock::now());
  addr_info->dial_failures++;

  auto peer_res = store.set(peer);
  if (peer_res.has_error()) {
    co_return peer_res.error();
  }

  auto d = retry_delay(addr_info->dial_failures, peer->persistent);
  if (d.count() != 0 && d.count() != retry_never) {
    co_spawn(
      io_context,
      [&, d]() -> asio::awaitable<void> {
        auto timer = boost::asio::steady_timer(io_context);
        defer _(nullptr, [&](...) { timer.cancel(); });
        timer.expires_after(d);
        auto timer_res =
          co_await (timer.async_wait(asio::use_awaitable) || done.async_receive(as_result(asio::use_awaitable)));
        if (timer_res.index() == 0) {
          co_await dial_waker.wake();
        }
      },
      asio::detached);
  } else {
    co_await dial_waker.wake();
  }
  co_return success();
}

auto PeerManager::dialed(const std::shared_ptr<NodeAddress>& address) -> asio::awaitable<Result<void>> {
  std::scoped_lock _(mtx);

  dialing.erase(address->node_id);

  NodeId upgrade_from_peer;
  for (auto pos = upgrading.begin(); pos != upgrading.end();) {
    if (pos->second == address->node_id) {
      upgrade_from_peer = pos->first;
      upgrading.erase(pos++);
    } else {
      ++pos;
    }
  }

  if (address->node_id == self_id) {
    co_return Error::format("rejecting connection to self ({})", address->node_id);
  }
  if (is_connected(address->node_id)) {
    co_return Error::format("peer {} is already connected", address->node_id);
  }
  if (options.max_connected > 0 && connected.size() >= options.max_connected) {
    if (upgrade_from_peer.empty() || connected.size() >= options.max_connected + options.max_connected_upgrade) {
      co_return Error("already connected to maximum number of peers");
    }
  }

  auto [peer, peer_ok] = store.get(address->node_id);
  if (!peer_ok) {
    co_return Error::format("peer {} was removed while dialing", address->node_id);
  }
  auto now = system_clock::to_time_t(system_clock::now());

  peer->inactive = false;
  peer->last_connected = now;
  auto addr_ok = peer->address_info.contains(address);
  auto addr_info = peer->address_info[address];

  if (addr_ok) {
    addr_info->dial_failures = 0;
    addr_info->last_dial_success = now;
  }

  auto set_res = store.set(peer);
  if (set_res.has_error()) {
    co_return set_res.error();
  }

  if (!upgrade_from_peer.empty() && options.max_connected > 0 && connected.size() >= options.max_connected) {
    auto [p, p_ok] = store.get(upgrade_from_peer);
    if (p_ok) {
      auto u = find_upgrade_candidate(peer->id, peer->score());
      if (!u.empty()) {
        upgrade_from_peer = u;
      }
    }
    evict[upgrade_from_peer] = true;
    co_await evict_waker.wake();
  }
  connected[peer->id] = PeerConnectionDirection::peer_connection_outgoing;

  co_return success();
}

auto PeerManager::is_connected(const NodeId& peer_id) -> const bool {
  return connected.contains(peer_id);
}

auto PeerManager::new_peer_info(const NodeId& id) -> std::shared_ptr<PeerInfo> {
  auto peer_info = std::make_shared<PeerInfo>(id);
  configure_peer(peer_info);
  return peer_info;
}

void PeerManager::configure_peer(std::shared_ptr<PeerInfo>& peer) {
  peer->persistent = options.is_persistent(peer->id);
  peer->fixed_score = options.peer_scores[peer->id];
}

auto PeerManager::find_upgrade_candidate(const NodeId& id, const PeerScore& score) -> NodeId {
  for (auto& [from, to] : upgrading) {
    if (to == id) {
      return from;
    }
  }

  auto ranked = store.ranked();
  for (auto i = ranked.size() - 1; i >= 0; i--) {
    auto candidate = ranked[i];
    if (candidate->id == id) {
    } else if (candidate->score() >= score) {
      return "";
    } else if (is_connected(candidate->id)) {
    } else if (evict.contains(candidate->id)) {
    } else if (evicting.contains(candidate->id)) {
    } else if (upgrading.contains(candidate->id) && !upgrading[candidate->id].empty()) {
    } else {
      return candidate->id;
    }
  }
  return "";
}

auto PeerManager::retry_delay(const uint32_t& failures, bool persistent) -> const std::chrono::nanoseconds {
  if (failures == 0) {
    return std::chrono::nanoseconds{0};
  }
  if (options.min_retry_time.count() == 0) {
    return std::chrono::nanoseconds{retry_never};
  }
  auto max_delay = options.max_retry_time;
  if (persistent && options.max_retry_time_persistent.count() > 0) {
    max_delay = options.max_retry_time_persistent;
  }

  auto delay = std::chrono::nanoseconds{options.min_retry_time.count() * failures};
  if (options.retry_time_jitter.count() > 0) {
    // TODO: plus random time
    // delay += time.Duration(m.rand.Int63n(int64(m.options.RetryTimeJitter)))
  }

  if (max_delay.count() > 0 && delay > max_delay) {
    delay = max_delay;
  }
  return delay;
}

auto PeerManager::try_dial_next() -> std::shared_ptr<NodeAddress> {
  std::scoped_lock _(mtx);

  if (options.max_connected > 0 &&
    connected.size() + dialing.size() >= options.max_connected + options.max_connected_upgrade) {
    return nullptr;
  }

  auto cinfo = get_connected_info();
  if (options.max_outgoing_connections > 0 && cinfo.outgoing >= options.max_outgoing_connections) {
    return nullptr;
  }

  for (auto& peer : store.ranked()) {
    if (dialing.contains(peer->id) || is_connected(peer->id)) {
      continue;
    }

    if (!(peer->last_disconnected == 0) &&
      std::chrono::duration_cast<nanoseconds>(system_clock::now() -
        system_clock::from_time_t(peer->last_disconnected)) < options.disconnect_cooldown_period) {
      continue;
    }

    for (auto& [_, addr_info] : peer->address_info) {
      if (std::chrono::duration_cast<nanoseconds>(
            system_clock::now() - system_clock::from_time_t(addr_info->dial_failures)) <
        retry_delay(addr_info->dial_failures, peer->persistent)) {
        continue;
      }

      auto [id, ok] = store.resolve(addr_info->address);
      if (ok && (is_connected(id) || dialing.contains(id))) {
        continue;
      }

      if (options.max_connected > 0 && connected.size() >= options.max_connected) {
        auto upgrade_from_peer = find_upgrade_candidate(peer->id, peer->score());
        if (upgrade_from_peer.empty()) {
          return nullptr;
        }
        upgrading[upgrade_from_peer] = peer->id;
      }

      dialing[peer->id] = true;
      return addr_info->address;
    }
  }
  return nullptr;
}

auto PeerManager::broadcast(Chan<std::monostate>& done, const std::shared_ptr<PeerUpdate>& peer_update)
  -> asio::awaitable<void> {
  for (auto& [_, sub] : subscriptions) {
    auto res = co_await (done.async_receive(as_result(asio::use_awaitable)) ||
      sub->reactor_updates_ch.async_send(boost::system::error_code{}, peer_update, asio::use_awaitable));

    if (res.index() == 0) {
      co_return;
    }
  }
}

auto PeerManager::get_connected_info() -> ConnectionStats {
  ConnectionStats out{};
  for (auto& [_, direction] : connected) {
    switch (direction) {
    case PeerConnectionDirection::peer_connection_incoming:
      out.incoming++;
      break;
    case PeerConnectionDirection::peer_connection_outgoing:
      out.outgoing++;
      break;
    }
  }
  return out;
}

auto PeerManager::disconnected(Chan<std::monostate>& done, const NodeId& peer_id) -> asio::awaitable<void> {
  std::scoped_lock _(mtx);

  auto ready_ok = _ready.contains(peer_id);
  connected.erase(peer_id);
  upgrading.erase(peer_id);
  evict.erase(peer_id);
  evicting.erase(peer_id);
  _ready.erase(peer_id);

  auto [peer, get_ok] = store.get(peer_id);
  if (get_ok) {
    peer->last_disconnected = system_clock::to_time_t(system_clock::now());
    store.set(peer);

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        auto timer = boost::asio::steady_timer(io_context);
        timer.expires_after(options.disconnect_cooldown_period);
        auto res =
          co_await (timer.async_wait(asio::use_awaitable) || done.async_receive(as_result(asio::use_awaitable)));
        if (res.index() == 0) {
          co_await dial_waker.wake();
        }
      },
      asio::detached);
  }

  if (ready_ok) {
    co_await broadcast(done, std::make_shared<PeerUpdate>(peer_id, peer_status_down));
  }

  co_await dial_waker.wake();
}

} //namespace tendermint::p2p
