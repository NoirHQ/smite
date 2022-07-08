// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <tendermint/p2p/address.h>
#include <tendermint/p2p/channel.h>
#include <tendermint/p2p/types.h>
#include <tendermint/p2p/waker.h>
#include <tendermint/types/node_id.h>
#include <eo/core.h>
#include <limits>

namespace tendermint::p2p {

const int64_t retry_never = std::numeric_limits<int64_t>::max();

using PeerStatus = std::string;

const PeerStatus peer_status_up = "up";
const PeerStatus peer_status_down = "down";
const PeerStatus peer_status_good = "good";
const PeerStatus peer_status_bad = "bad";

class PeerUpdate {
public:
  PeerUpdate(const NodeId& node_id, const PeerStatus& status, const ChannelIdSet& channels)
    : node_id(node_id), status(status), channels(channels) {}
  PeerUpdate(const NodeId& node_id, const PeerStatus& status): node_id(node_id), status(status) {}

public:
  NodeId node_id;
  PeerStatus status;
  ChannelIdSet channels;
};

class PeerUpdates {
public:
  eo::chan<std::shared_ptr<PeerUpdate>> router_updates_ch;
  eo::chan<std::shared_ptr<PeerUpdate>> reactor_updates_ch;
};

enum class PeerConnectionDirection {
  peer_connection_incoming = 1,
  peer_connection_outgoing = 2
};

using PeerScore = uint16_t;

constexpr PeerScore peer_score_persistent = std::numeric_limits<uint16_t>::max();
constexpr PeerScore max_peer_score_not_persistent = peer_score_persistent - 1;

// PeerManagerOptions specifies options for a PeerManager.
class PeerManagerOptions {
public:
  auto is_persistent(const NodeId& id) -> bool;

public:
  std::vector<NodeId> persistent_peers;
  uint16_t max_peers;
  uint16_t max_connected;
  uint16_t max_outgoing_connections;
  uint16_t max_connected_upgrade;
  std::chrono::nanoseconds min_retry_time;
  std::chrono::nanoseconds max_retry_time;
  std::chrono::nanoseconds max_retry_time_persistent;
  std::chrono::nanoseconds retry_time_jitter;
  std::chrono::nanoseconds disconnect_cooldown_period;
  std::map<NodeId, PeerScore> peer_scores;
  std::set<NodeId> private_peers;
  NodeAddress self_address;
  std::map<NodeId, bool> persistent_peers_lookup;
};

class PeerAddressInfo {
public:
  uint32_t dial_failures;
  std::time_t last_dial_failure;
  std::time_t last_dial_success;
  std::shared_ptr<NodeAddress> address;
};

class PeerInfo {
public:
  PeerInfo(const NodeId& id): id(id) {}
  PeerInfo(PeerInfo& peer)
    : address_info(peer.address_info),
      id(peer.id),
      inactive(peer.inactive),
      last_disconnected(peer.last_disconnected),
      last_connected(peer.last_connected),
      persistent(peer.persistent),
      fixed_score(peer.fixed_score),
      mutable_score(peer.mutable_score),
      height(peer.height) {}
  auto score() -> PeerScore;
  auto validate() -> noir::Result<void>;

public:
  std::map<std::shared_ptr<NodeAddress>, std::shared_ptr<PeerAddressInfo>> address_info;
  NodeId id;
  bool inactive;
  std::time_t last_disconnected;
  std::time_t last_connected;
  bool persistent;
  PeerScore fixed_score;
  int64_t mutable_score;

private:
  int64_t height;
};

class PeerStore {
public:
  auto get(const NodeId& peer_id) -> std::tuple<std::shared_ptr<PeerInfo>, bool>;
  auto set(const std::shared_ptr<PeerInfo>& peer) -> noir::Result<void>;
  auto ranked() -> std::vector<std::shared_ptr<PeerInfo>>;
  auto resolve(const std::shared_ptr<NodeAddress>& addr) -> std::tuple<NodeId, bool>;

  // Db db;
  std::map<NodeId, std::shared_ptr<PeerInfo>> peers;

private:
  std::map<std::shared_ptr<NodeAddress>, NodeId> index;
  std::vector<std::shared_ptr<PeerInfo>> _ranked;
};

struct ConnectionStats {
  uint16_t incoming;
  uint16_t outgoing;
};

class PeerManager {
public:
  auto has_max_peer_capacity() -> bool;
  auto errored(const NodeId& peer_id, Error& error) -> boost::asio::awaitable<void>;
  void process_peer_event(eo::chan<std::monostate>& done, const std::shared_ptr<PeerUpdate>& pu);
  auto accepted(const NodeId& peer_id) -> boost::asio::awaitable<noir::Result<void>>;
  auto ready(eo::chan<std::monostate>& done, const NodeId& peer_id, ChannelIdSet& channels)
    -> boost::asio::awaitable<void>;
  auto dial_next(eo::chan<std::monostate>& done)
    -> boost::asio::awaitable<noir::Result<std::shared_ptr<NodeAddress>>>;
  auto evict_next(eo::chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<NodeId>>;
  auto inactivate(const NodeId& peer_id) -> noir::Result<void>;
  auto dial_failed(eo::chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
    -> boost::asio::awaitable<noir::Result<void>>;
  auto dialed(const std::shared_ptr<NodeAddress>& address) -> boost::asio::awaitable<noir::Result<void>>;
  auto is_connected(const NodeId& peer_id) -> const bool;
  auto new_peer_info(const NodeId& id) -> std::shared_ptr<PeerInfo>;
  void configure_peer(std::shared_ptr<PeerInfo>& peer);
  auto find_upgrade_candidate(const NodeId& id, const PeerScore& score) -> NodeId;
  auto broadcast(eo::chan<std::monostate>& done, const std::shared_ptr<PeerUpdate>& peer_update)
    -> boost::asio::awaitable<void>;
  auto try_dial_next() -> std::shared_ptr<NodeAddress>;
  auto try_evict_next() -> noir::Result<NodeId>;
  auto retry_delay(const uint32_t& failures, bool persistent) -> const std::chrono::nanoseconds;
  auto get_connected_info() -> ConnectionStats;
  auto disconnected(eo::chan<std::monostate>& done, const NodeId& peer_id) -> boost::asio::awaitable<void>;

public:
  boost::asio::io_context& io_context;
  NodeId self_id;
  PeerManagerOptions options;
  Waker dial_waker;
  Waker evict_waker;

  std::mutex mtx;
  PeerStore store;
  std::map<std::shared_ptr<PeerUpdates>, std::shared_ptr<PeerUpdates>>
    subscriptions; // keyed by struct identity (address)
  std::map<NodeId, bool> dialing; // peers being dialed (dial_next → dialed/dial_fail)
  std::map<NodeId, NodeId> upgrading; // peers claimed for upgrade (dial_next → dialed/dial_fail)
  std::map<NodeId, PeerConnectionDirection> connected; // // connected peers (dialed/accepted → disconnected)
  std::map<NodeId, bool> _ready; // TODO: naming convention // ready peers (ready → disconnected)
  std::map<NodeId, bool> evict; // peers scheduled for eviction (connected → evict_next)
  std::map<NodeId, bool> evicting; // peers being evicted (evict_next → disconnected)
};
} // namespace tendermint::p2p