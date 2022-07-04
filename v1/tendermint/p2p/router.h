// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <tendermint/p2p/address.h>
#include <tendermint/p2p/channel.h>
#include <tendermint/p2p/conn/connection.h>
#include <tendermint/p2p/peermanager.h>
#include <tendermint/p2p/queue.h>
#include <tendermint/p2p/transport_mconn.h>
#include <tendermint/p2p/types.h>
#include <chrono>
#include <shared_mutex>
#include <string>

namespace tendermint::p2p {

const std::size_t queue_buffer_default = 32;

// RouterOptions specifies options for a Router.
class RouterOptions {
public:
  auto validate() -> noir::Result<void>;

public:
  std::function<noir::Result<void>(noir::Chan<std::monostate>& done, const std::string& address)> filter_peer_by_ip;
  std::function<noir::Result<void>(noir::Chan<std::monostate>& done, const NodeId& id)> filter_peer_by_id;
  std::function<int32_t(void)> num_current_dials;
  // resolve_timeout is the timeout for resolving NodeAddress URLs. 0 means no timeout.
  std::chrono::milliseconds resolve_timeout;
  // dial_timeout is the timeout for dialing a peer. 0 means no timeout.
  std::chrono::milliseconds dial_timeout;
  // HandshakeTimeout is the timeout for handshaking with a peer. 0 means no timeout.
  std::chrono::milliseconds handshake_timeout;
  // queue_type must be, "priority", or "fifo". Defaults to "fifo".
  std::string queue_type;
  // max_incoming_connection_attempts rate limits the number of incoming connection attempts per IP address. Defaults to
  // 100.
  uint32_t max_incoming_connection_attempts;
  // incoming_connection_window describes how often an IP address can attempt to create a new connection. Defaults to 10
  // milliseconds, and cannot be less than 1 millisecond.
  std::chrono::milliseconds incoming_connection_window;
};

class Router {
public:
  auto start(noir::Chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<void>>;
  auto open_channel(noir::Chan<std::monostate>& done, std::shared_ptr<conn::ChannelDescriptor>& ch_desc)
    -> noir::Result<std::shared_ptr<Channel>>;
  static auto new_router(boost::asio::io_context& context,
    Bytes& priv_key,
    std::shared_ptr<PeerManager>& peer_manager,
    NodeInfo& node_info,
    std::shared_ptr<MConnTransport>& transport,
    const std::string& endpoint,
    RouterOptions& options) -> noir::Result<std::shared_ptr<Router>> {
    auto res = options.validate();
    if (res.has_error()) {
      return res.error();
    }
    return std::shared_ptr<Router>(
      new Router(context, priv_key, peer_manager, node_info, transport, endpoint, options));
  }

private:
  Router(boost::asio::io_context& context,
    Bytes& priv_key,
    std::shared_ptr<PeerManager>& peer_manager,
    NodeInfo& node_info,
    std::shared_ptr<MConnTransport>& transport,
    const std::string& endpoint,
    RouterOptions& options)
    : io_context(context),
      priv_key(priv_key),
      peer_manager(peer_manager),
      node_info(node_info),
      transport(transport),
      endpoint(endpoint),
      options(options) {}

  auto route_channel(noir::Chan<std::monostate>& done,
    ChannelId ch_id,
    std::shared_ptr<noir::Chan<EnvelopePtr>> out_ch,
    std::shared_ptr<noir::Chan<PeerErrorPtr>> err_ch,
    std::shared_ptr<google::protobuf::Message> wrapper) -> boost::asio::awaitable<noir::Result<void>>;
  auto dial_peers(noir::Chan<std::monostate>& done) -> boost::asio::awaitable<void>;
  auto evict_peers(noir::Chan<std::monostate>& done) -> boost::asio::awaitable<void>;
  auto accept_peers(noir::Chan<std::monostate>& done, std::shared_ptr<MConnTransport>& transport)
    -> boost::asio::awaitable<void>;
  auto num_concurrent_dials() const -> int32_t;
  auto filter_peers_ip(noir::Chan<std::monostate>& done, const std::string& endpoint) -> noir::Result<void>;
  auto filter_peers_id(noir::Chan<std::monostate>& done, const NodeId& node_id) -> noir::Result<void>;
  auto open_connection(noir::Chan<std::monostate>& done, std::shared_ptr<MConnConnection> conn)
    -> boost::asio::awaitable<void>;
  auto connect_peer(noir::Chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
    -> boost::asio::awaitable<void>;
  auto get_or_make_queue(const NodeId& peer_id, ChannelIdSet& channels) -> std::shared_ptr<FifoQueue>;
  auto dial_peer(noir::Chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
    -> boost::asio::awaitable<noir::Result<std::shared_ptr<MConnConnection>>>;
  auto handshake_peer(noir::Chan<std::monostate>& done,
    std::shared_ptr<MConnConnection>& conn,
    const NodeId& expected_id) -> boost::asio::awaitable<std::tuple<NodeInfo, noir::Result<void>>>;
  auto route_peer(noir::Chan<std::monostate>& done,
    const NodeId& peer_id,
    std::shared_ptr<MConnConnection>& conn,
    ChannelIdSet&& channels) -> boost::asio::awaitable<void>;
  auto receive_peer(noir::Chan<std::monostate>& done, const NodeId& peer_id, std::shared_ptr<MConnConnection> conn)
    -> boost::asio::awaitable<noir::Result<void>>;
  auto send_peer(noir::Chan<std::monostate>& done,
    const NodeId& peer_id,
    std::shared_ptr<MConnConnection>& conn,
    std::shared_ptr<FifoQueue>& peer_queue) -> boost::asio::awaitable<noir::Result<void>>;
  auto run_with_peer_mutex(std::function<boost::asio::awaitable<noir::Result<void>>(void)>&& fn)
    -> boost::asio::awaitable<noir::Result<void>>;

private:
  boost::asio::io_context& io_context;
  RouterOptions options;
  noir::Bytes priv_key;
  std::shared_ptr<PeerManager> peer_manager;
  std::vector<std::shared_ptr<conn::ChannelDescriptor>> ch_descs;
  std::shared_ptr<MConnTransport> transport;
  std::string endpoint;

  std::shared_mutex peer_mtx;
  std::map<NodeId, std::shared_ptr<FifoQueue>> peer_queues; // outbound messages per peer for all channels
  // the channels that the peer queue has open
  std::map<NodeId, ChannelIdSet> peer_channels;
  NodeInfo node_info;

  std::shared_mutex channel_mtx;
  std::map<ChannelId, std::shared_ptr<FifoQueue>> channel_queues;
  std::map<ChannelId, std::shared_ptr<google::protobuf::Message>> channel_messages;
};
} //namespace tendermint::p2p
