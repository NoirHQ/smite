// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <noir/net/tcp_listener.h>
#include <boost/asio/io_context.hpp>
#include <tendermint/p2p/conn/connection.h>
#include <tendermint/p2p/types.h>
#include <tendermint/types/node_info.h>
#include <numeric>
#include <utility>

namespace tendermint::p2p {

// mConnMessage passes MConnection messages through internal channels.
struct MConnMessage {
  ChannelId channel_id;
  std::shared_ptr<noir::Bytes> payload;
};

class MConnConnection {
public:
  MConnConnection(boost::asio::io_context& io_context,
    std::shared_ptr<noir::net::Conn<noir::net::TcpConn>>& conn,
    conn::MConnConfig& mconn_config,
    std::vector<conn::ChannelDescriptorPtr>& channel_descs)
    : io_context(io_context),
      conn(conn),
      mconn_config(mconn_config),
      channel_descs(channel_descs),
      receive_ch(io_context),
      error_ch(io_context, 1),
      close_ch(io_context) {}
  auto handshake_inner(NodeInfo& node_info, noir::Bytes& priv_key)
    -> boost::asio::awaitable<Result<std::tuple<std::shared_ptr<conn::MConnection>, NodeInfo, noir::Bytes>>>;
  auto handshake(noir::Chan<std::monostate>& done, NodeInfo& node_info, noir::Bytes& priv_key)
    -> boost::asio::awaitable<Result<std::tuple<NodeInfo, noir::Bytes>>>;
  auto send_message(noir::Chan<std::monostate>& done, ChannelId ch_id, std::shared_ptr<noir::Bytes>& msg)
    -> boost::asio::awaitable<Result<void>>;
  auto receive_message(noir::Chan<std::monostate>& done)
    -> boost::asio::awaitable<noir::Result<std::tuple<ChannelId, std::shared_ptr<noir::Bytes>>>>;
  auto remote_endpoint() -> std::string;
  auto close() -> noir::Result<void>;

private:
  boost::asio::io_context& io_context;
  std::shared_ptr<noir::net::Conn<noir::net::TcpConn>> conn;
  conn::MConnConfig mconn_config;
  std::vector<conn::ChannelDescriptorPtr> channel_descs;
  noir::Chan<MConnMessage> receive_ch;
  noir::Chan<noir::Error> error_ch;
  noir::Chan<std::monostate> close_ch;

  std::shared_ptr<conn::MConnection> mconn; // set during Handshake()
};

// MConnTransportOptions sets options for MConnTransport.
struct MConnTransportOptions {
  // MaxAcceptedConnections is the maximum number of simultaneous accepted
  // (incoming) connections. Beyond this, new connections will block until
  // a slot is free. 0 means unlimited.
  //
  // FIXME: We may want to replace this with connection accounting in the
  // Router, since it will need to do e.g. rate limiting and such as well.
  // But it might also make sense to have per-transport limits.
  std::uint32_t max_accepted_connections;
};

// MConnTransport is a Transport implementation using the current multiplexed
// Tendermint protocol ("MConn").
class MConnTransport {
public:
  MConnTransport(boost::asio::io_context& io_context, conn::MConnConfig& mconn_config, MConnTransportOptions& options)
    : io_context(io_context), mconn_config(mconn_config), options(options), done_ch(io_context) {}

  auto listen(const std::string& endpoint) -> noir::Result<void>;
  auto accept(noir::Chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<MConnConnection>>;
  auto dial(const std::string& endpoint) -> boost::asio::awaitable<noir::Result<MConnConnection>>;
  auto close() -> noir::Result<void>;
  void add_channel_descriptor(std::vector<conn::ChannelDescriptorPtr>& channel_descs);
  static auto validate_endpoint(const std::string& endpoint) -> noir::Result<void>;

private:
  boost::asio::io_context& io_context;
  MConnTransportOptions options;
  conn::MConnConfig mconn_config;
  std::vector<conn::ChannelDescriptorPtr> channel_descs;
  noir::Chan<std::monostate> done_ch;
  std::shared_ptr<noir::net::TcpListener> listener;
};

} //namespace tendermint::p2p
