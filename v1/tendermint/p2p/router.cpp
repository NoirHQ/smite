// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/defer.h>
#include <noir/common/wait_group.h>
#include <tendermint/p2p/router.h>
#include <thread>

namespace tendermint::p2p {

using namespace noir;

static auto to_channel_ids(const Bytes& bytes) -> ChannelIdSet {
  ChannelIdSet c{};
  for (auto& b : bytes) {
    c.insert(uint16_t(b));
  }
  return c;
}

auto RouterOptions::validate() -> Result<void> {
  if (queue_type.empty()) {
    queue_type = "fifo";
  } else if (queue_type == "fifo" || queue_type == "priority") {
  } else {
    return Error::format("queue type {} is not supported", queue_type);
  }

  if (incoming_connection_window == std::chrono::milliseconds{0}) {
    incoming_connection_window = std::chrono::milliseconds{100};
  }
  if (max_incoming_connection_attempts == 0) {
    max_incoming_connection_attempts = 100;
  }
  return success();
}

auto Router::start(Chan<std::monostate>& done) -> asio::awaitable<Result<void>> {
  // TODO: setupQueueFactory

  auto res = co_await transport->listen(endpoint);
  if (res.has_error()) {
    co_return res.error();
  }

  co_spawn(io_context, dial_peers(done), asio::detached);
  co_spawn(io_context, evict_peers(done), asio::detached);
  co_spawn(io_context, accept_peers(done, transport), asio::detached);

  co_return success();
}

// dial_peers maintains outbound connections to peers by dialing them.
auto Router::dial_peers(Chan<std::monostate>& done) -> asio::awaitable<void> {
  // TODO: logger
  // r.logger.Debug("starting dial routine")
  Chan<std::shared_ptr<NodeAddress>> addresses(io_context);
  WaitGroup wg(io_context);

  for (auto i = 0; i < num_concurrent_dials(); i++) {
    wg.add();

    co_spawn(
      io_context,
      [&]() -> boost::asio::awaitable<void> {
        for (;;) {
          auto receive_res = co_await (
            done.async_receive(as_result(asio::use_awaitable)) || addresses.async_receive(asio::use_awaitable));
          if (receive_res.index() == 0) {
            break;
          } else if (receive_res.index() == 1) {
            auto address = std::get<1>(receive_res);
            co_await connect_peer(done, address);
          } else {
            co_return;
          }
        }
        co_await wg.done();
      },
      asio::detached);
  }

  for (;;) {
    auto dial_res = peer_manager->dial_next(done);
    if (dial_res.has_error() && dial_res.error() == err_canceled) {
      break;
    }
    auto address = dial_res.value();
    if (!address) {
      continue;
    }

    auto send_res = co_await (addresses.async_send(boost::system::error_code{}, address, asio::use_awaitable) ||
      done.async_receive(as_result(asio::use_awaitable)));
    if (send_res.index() == 0) {
      continue;
    } else if (send_res.index() == 1) {
      addresses.close();
      break;
    } else {
      co_return;
    }
  }

  co_await wg.wait();
}

auto Router::connect_peer(Chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
  -> asio::awaitable<void> {
  auto dial_res = co_await dial_peer(done, address);
  if (dial_res.has_error()) {
    if (dial_res.error() == err_canceled) {
      co_return;
    }
    // TODO: logger
    // r.logger.Debug("failed to dial peer", "peer", address, "err", err)
    auto failed_res = peer_manager->dial_failed(done, address);
    if (failed_res.has_error()) {
      // TODO: logger
      // r.logger.Error("failed to report dial failure", "peer", address, "err", err)
    }
    co_return;
  }

  auto conn = dial_res.value();
  auto [peer_info, handshake_res] = co_await handshake_peer(done, conn, address->node_id);
  if (handshake_res.has_error()) {
    if (handshake_res.error() == err_canceled) {
      conn->close();
      co_return;
    } else {
      // TODO
      // r.logger.Error("failed to handshake with peer", "peer", address, "err", err)
      auto failed_res = peer_manager->dial_failed(done, address);
      if (failed_res.has_error()) {
        // TODO: logger
        // r.logger.Error("failed to report dial failure", "peer", address, "err", err)
      }
      conn->close();
      co_return;
    }
  }

  auto run_res = run_with_peer_mutex([&]() -> Result<void> { return peer_manager->dialed(address); });
  if (run_res.has_error()) {
    // TODO
    // r.logger.Error("failed to dial peer", "op", "outgoing/dialing", "peer", address.NodeID, "err", err)
    co_await peer_manager->dial_waker.wake();
    conn->close();
    co_return;
  }

  co_spawn(io_context, route_peer(done, address->node_id, conn, to_channel_ids(peer_info.channels)), asio::detached);
}

auto Router::get_or_make_queue(const NodeId& peer_id, ChannelIdSet& channels) -> std::shared_ptr<FifoQueue> {
  std::unique_lock lock(peer_mtx);
  defer _(nullptr, [&](...) { lock.release(); });

  if (peer_queues.contains(peer_id)) {
    return peer_queues[peer_id];
  }

  // TODO: priority queue
  auto peer_queue = FifoQueue::new_fifo_queue(io_context, queue_buffer_default);
  peer_queues[peer_id] = peer_queue;
  peer_channels[peer_id] = channels;
  return peer_queue;
}

auto Router::dial_peer(Chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
  -> asio::awaitable<Result<std::shared_ptr<MConnConnection>>> {
  if (options.resolve_timeout.count() > 0) {
    // TODO: resolve timeout
  }

  // TODO
  // r.logger.Debug("resolving peer address", "peer", address)
  auto resolve_res = address->resolve(done);
  if (resolve_res.has_error()) {
    co_return Error::format("failed to resolve address {}: {}", address->to_string(), resolve_res.error());
  }
  auto endpoints = resolve_res.value();
  if (endpoints.size() == 0) {
    co_return Error::format("address {} did not resolve to any endpoints", address->to_string());
  }

  for (auto& endpoint : endpoints) {
    if (options.dial_timeout.count() > 0) {
      // TODO: dial timeout
    }

    auto dial_res = co_await transport->dial(endpoint);
    if (dial_res.has_error()) {
      // TODO: logger
      // r.logger.Debug("failed to dial endpoint", "peer", address.NodeID, "endpoint", endpoint, "err", err)
    } else {
      // TODO: logger
      // r.logger.Debug("dialed peer", "peer", address.NodeID, "endpoint", endpoint)
      co_return dial_res.value();
    }
    co_return Error("all endpoints failed");
  }
}

auto Router::num_concurrent_dials() const -> int32_t {
  if (!options.num_current_dials) {
    return std::thread::hardware_concurrency() * 32;
  }
  return options.num_current_dials();
}

auto Router::evict_peers(Chan<std::monostate>& done) -> asio::awaitable<void> {
  auto evict_res = co_await peer_manager->evict_next(done);

  if (evict_res.has_error()) {
    if (evict_res.error() != err_canceled) {
      // TODO: logger
      // r.logger.Error("failed to find next peer to evict", "err", err)
    }
    co_return;
  }
  // TODO: logger
  // r.logger.Info("evicting peer", "peer", peerID)

  auto peer_id = evict_res.value();

  std::shared_lock lock(peer_mtx);
  auto ok = peer_queues.contains(peer_id);
  auto queue = peer_queues[peer_id];
  lock.release();

  if (ok) {
    queue->close();
  }
}

// accept_peers accepts inbound connections from peers on the given transport,
// and spawns coroutines that route messages to/from them.
auto Router::accept_peers(noir::Chan<std::monostate>& done, std::shared_ptr<MConnTransport>& transport)
  -> boost::asio::awaitable<void> {
  // TODO: logger
  // r.logger.Debug("starting accept routine", "transport", transport)
  for (;;) {
    auto res = co_await transport->accept(done);
    if (res.has_error()) {
      auto err = res.error();
      if (err == err_canceled || err == err_deadline_exceeded) {
        // TODO: logger
        // r.logger.Debug("stopping accept routine", "transport", transport, "err", "context canceled")
        co_return;
      } else if (err == err_eof) {
        // TODO: logger
        // r.logger.Debug("stopping accept routine", "transport", transport, "err", "EOF")
        co_return;
      } else {
        // TODO: logger
        // r.logger.Error("failed to accept connection", "transport", transport, "err", err)
        continue;
      }
    }

    auto conn = res.value();
    co_spawn(io_context, open_connection(done, conn), asio::detached);
  }
}

auto Router::open_connection(noir::Chan<std::monostate>& done, std::shared_ptr<MConnConnection> conn)
  -> asio::awaitable<void> {
  auto re = conn->remote_endpoint();

  auto filter_by_ip_res = filter_peers_ip(done, re);
  if (filter_by_ip_res.has_error()) {
    // TODO: logger
    // 	r.logger.Debug("peer filtered by IP", "ip", incomingIP.String(), "err", err)
    co_return;
  }
  // FIXME: The peer manager may reject the peer during Accepted() after we've handshaked with the peer (to find out
  // which peer it is). However, because the handshake has no ack, the remote peer will think the handshake was
  // successful and start sending us messages.
  //
  // This can cause problems in tests, where a disconnection can cause the local node to immediately redial, while the
  // remote node may not have completed the disconnection yet and therefore reject the reconnection attempt (since it
  // thinks we're still connected from before).
  //
  // The Router should do the handshake and have a final ack/fail message to make sure both ends have accepted the
  // connection, such that it can be coordinated with the peer manager.
  auto [peer_info, handshake_res] = co_await handshake_peer(done, conn, "");

  if (handshake_res.has_error()) {
    auto handshake_err = handshake_res.error();
    if (handshake_err == err_canceled) {
      co_return;
    } else {
      // TODO: logger
      // r.logger.Error("peer handshake failed", "endpoint", conn, "err", err)
      co_return;
    }
  }
  auto filter_by_id_res = filter_peers_id(done, peer_info.node_id);
  if (filter_by_id_res.has_error()) {
    // TODO: logger
    // r.logger.Debug("peer filtered by node ID", "node", peerInfo.NodeID, "err", err)
    co_return;
  }

  auto run_res =
    run_with_peer_mutex([&, node_id = peer_info.node_id]() -> Result<void> { return peer_manager->accepted(node_id); });
  if (run_res.has_error()) {
    // TODO: logger
    // r.logger.Error("failed to accept connection", "op", "incoming/accepted", "peer", peerInfo.NodeID, "err", err)
    co_return;
  }

  co_await route_peer(done, peer_info.node_id, conn, to_channel_ids(peer_info.channels));
}

auto Router::filter_peers_ip(noir::Chan<std::monostate>& done, const std::string& endpoint) -> noir::Result<void> {
  if (!options.filter_peer_by_ip) {
    return success();
  }
  return options.filter_peer_by_ip(done, endpoint);
}

auto Router::filter_peers_id(noir::Chan<std::monostate>& done, const NodeId& id) -> noir::Result<void> {
  if (!options.filter_peer_by_id) {
    return success();
  }
  return options.filter_peer_by_id(done, id);
}

auto Router::handshake_peer(Chan<std::monostate>& done,
  std::shared_ptr<MConnConnection>& conn,
  const NodeId& expected_id) -> asio::awaitable<std::tuple<NodeInfo, Result<void>>> {
  auto [peer_info, peer_key, handshake_res] = co_await conn->handshake(done, node_info, priv_key);

  if (handshake_res.has_error()) {
    co_return std::make_tuple(peer_info, handshake_res.error());
  }
  auto valid_res = peer_info.validate();
  if (valid_res.has_error()) {
    co_return std::make_tuple(peer_info, Error::format("invalid handshake NodeInfo: {}", valid_res.error()));
  }

  if (node_id_from_pubkey(*peer_key) == peer_info.node_id) {
    co_return std::make_tuple(peer_info,
      Error::format("peer's public key did not match its node ID {} (expected {})",
        peer_info.node_id,
        node_id_from_pubkey(*peer_key)));
  }

  auto comp_res = node_info.compatible_with(peer_info);
  if (comp_res.has_error()) {
    auto inactivate_res = peer_manager->inactivate(peer_info.node_id);
    if (inactivate_res.has_error()) {
      co_return std::make_tuple(peer_info,
        Error::format("problem inactivating peer {}: {}", peer_info.node_id, inactivate_res.error()));
    }
    // TODO: ErrRejected
    co_return std::make_tuple(peer_info, Error("rejected"));
  }
  co_return std::make_tuple(peer_info, success());
}

auto Router::run_with_peer_mutex(std::function<noir::Result<void>(void)>&& fn) -> noir::Result<void> {
  std::unique_lock lock(peer_mtx);
  return fn();
}

// routePeer routes inbound and outbound messages between a peer and the reactor channels.
// It will close the given connection and send queue when done, or if they are closed elsewhere it will cause this
// method to shut down and return.
auto Router::route_peer(
  Chan<std::monostate>& done, const NodeId& peer_id, std::shared_ptr<MConnConnection>& conn, ChannelIdSet&& channels)
  -> asio::awaitable<void> {
  peer_manager->ready(done, peer_id, channels);

  auto send_queue = get_or_make_queue(peer_id, channels);

  // TODO: logger
  // r.logger.Info("peer connected", "peer", peerID, "endpoint", conn)
  auto res_ch = std::make_shared<Chan<Result<void>>>(io_context, 2);

  co_spawn(
    io_context,
    [&, res_ch]() -> asio::awaitable<void> {
      auto receive_res = co_await receive_peer(done, peer_id, conn);
      co_await (res_ch->async_send(boost::system::error_code{}, receive_res, asio::use_awaitable) ||
        done.async_receive(as_result(asio::use_awaitable)));
    },
    asio::detached);
  co_spawn(
    io_context,
    [&, res_ch]() -> asio::awaitable<void> {
      auto send_res = co_await send_peer(done, peer_id, conn, send_queue);
      co_await (res_ch->async_send(boost::system::error_code{}, send_res, asio::use_awaitable) ||
        done.async_receive(as_result(asio::use_awaitable)));
    },
    asio::detached);

  auto res = co_await res_ch->async_receive(asio::use_awaitable);
  conn->close();
  send_queue->close();

  res = co_await res_ch->async_receive(asio::use_awaitable);
  if (!res.has_error() || res.error() == err_eof) {
    // TODO: logger
    // r.logger.Info("peer disconnected", "peer", peerID, "endpoint", conn)
  } else {
    // TODO: logger
    // r.logger.Error("peer failure", "peer", peerID, "endpoint", conn, "err", err)
  }
}

// receive_peer receives inbound messages from a peer, deserializes them and passes them on to the appropriate channel.
auto Router::receive_peer(Chan<std::monostate>& done, const NodeId& peer_id, std::shared_ptr<MConnConnection> conn)
  -> asio::awaitable<Result<void>> {
  for (;;) {
    auto [ch_id, bz, receive_res] = co_await conn->receive_message(done);
    if (receive_res.has_error()) {
      co_return receive_res.error();
    }
    std::shared_lock lock(channel_mtx);
    auto contained = channel_queues.contains(ch_id);
    auto queue = channel_queues[ch_id];
    auto message_type = channel_messages[ch_id];
    lock.release();

    if (!contained) {
      // TODO: logger
      // r.logger.Debug("dropping message for unknown channel", "peer", peerID, "channel", chID)
      continue;
    }
    auto message = message_type->New();
    auto parsed = message->ParseFromArray(bz->data(), bz->size());
    if (!parsed) {
      // TODO: logger
      // r.logger.Error("message decoding failed, dropping message", "peer", peerID, "err", err)
      continue;
    }

    // TODO: wrapper
    // if wrapper, ok := msg.(Wrapper); ok {
    //   msg, err = wrapper.Unwrap()
    //   if err != nil {
    //     r.logger.Error("failed to unwrap message", "err", err)
    //     continue
    //   }
    // }

    auto queue_res = co_await (
      queue->queue_ch->async_send(boost::system::error_code{}, std::make_shared<Envelope>(), asio::use_awaitable) ||
      queue->close_ch.async_receive(as_result(asio::use_awaitable)) ||
      done.async_receive(as_result(asio::use_awaitable)));

    if (queue_res.index() == 0) {
      // TODO: logger
      // r.logger.Debug("received message", "peer", peerID, "message", msg)
    } else if (queue_res.index() == 1) {
      // TODO: logger
      // r.logger.Debug("channel closed, dropping message", "peer", peerID, "channel", chID)
    } else if (queue_res.index() == 2) {
      co_return std::get<2>(queue_res).error();
    } else {
      co_return err_unreachable;
    }
  }
}

// send_peer sends queued messages to a peer.
auto send_peer(Chan<std::monostate>& done,
  const NodeId& peer_id,
  std::shared_ptr<MConnConnection>& conn,
  std::shared_ptr<FifoQueue>& peer_queue) -> asio::awaitable<Result<void>> {
  for (;;) {
    auto res = co_await (peer_queue->queue_ch->async_receive(asio::use_awaitable) ||
      peer_queue->close_ch.async_receive(as_result(asio::use_awaitable)) ||
      done.async_receive(as_result(asio::use_awaitable)));

    if (res.index() == 0) {
      auto envelope = std::get<0>(res);
      if (!envelope->message) {
        // TODO: logger
        // r.logger.Error("dropping nil message", "peer", peerID)
        continue;
      }

      auto msg_size = envelope->message->ByteSizeLong();
      auto bz = std::make_shared<Bytes>(msg_size);
      envelope->message->SerializeToArray(bz->data(), bz->size());

      auto send_res = co_await conn->send_message(done, envelope->channel_id, bz);
      if (send_res.has_error()) {
        co_return send_res.error();
      }
      // TODO: logger
      // r.logger.Debug("sent message", "peer", envelope.To, "message", envelope.Message)
    } else if (res.index() == 1) {
      co_return std::get<1>(res).error();
    } else if (res.index() == 2) {
      co_return std::get<2>(res).error();
    } else {
      co_return err_unreachable;
    }
  }
}

auto Router::open_channel(Chan<std::monostate>& done, std::shared_ptr<conn::ChannelDescriptor>& ch_desc)
  -> Result<std::shared_ptr<Channel>> {
  std::unique_lock lock(channel_mtx);

  if (channel_queues.contains(ch_desc->id)) {
    return Error::format("channel {} already exists", ch_desc->id);
  }
  ch_descs.push_back(ch_desc);
  // TODO: priority queue
  auto queue = FifoQueue::new_fifo_queue(io_context, ch_desc->recv_buffer_capacity);
  auto out_ch = std::make_shared<Chan<EnvelopePtr>>(io_context, ch_desc->recv_buffer_capacity);
  auto err_ch = std::make_shared<Chan<PeerErrorPtr>>(io_context, ch_desc->recv_buffer_capacity);

  auto channel = Channel::new_channel(io_context, ch_desc->id, queue->queue_ch, out_ch, err_ch);
  channel->name = ch_desc->name;

  channel_queues.insert({ch_desc->id, queue});
  channel_messages.insert({ch_desc->id, ch_desc->message_type});

  node_info.add_channels(ch_desc->id);
  transport->add_channel_descriptor(std::vector<std::shared_ptr<conn::ChannelDescriptor>>{ch_desc});

  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      co_await route_channel(done, ch_desc->id, out_ch, err_ch, ch_desc->message_type);
      queue->close();
    },
    asio::detached);

  return channel;
}

// route_channel receives outbound channel messages and routes them to the appropriate peer.
// It also receives peer errors and reports them to the peer manager.
// It returns when either the outbound channel or error channel is closed, or the Router is stopped.
// wrapper is an optional message wrapper for messages, see Wrapper for details.
auto Router::route_channel(Chan<std::monostate>& done,
  ChannelId ch_id,
  std::shared_ptr<Chan<EnvelopePtr>> out_ch,
  std::shared_ptr<Chan<PeerErrorPtr>> err_ch,
  std::shared_ptr<google::protobuf::Message> wrapper) -> asio::awaitable<noir::Result<void>> {
  for (;;) {
    auto res = co_await (out_ch->async_receive(asio::use_awaitable) || err_ch->async_receive(asio::use_awaitable) ||
      done.async_receive(as_result(asio::use_awaitable)));

    if (res.index() == 0) {
      auto envelope = std::get<0>(res);
      envelope->channel_id = ch_id;
      // TODO: wrapper
      // wrap the message in a wrapper message, if requested
      // if wrapper != nil {
      //   msg := proto.Clone(wrapper)
      //   if err := msg.(Wrapper).Wrap(envelope.Message); err != nil {
      //     r.logger.Error("failed to wrap message", "channel", chID, "err", err)
      //     continue
      //   }
      //   envelope.Message = msg
      // }

      std::vector<std::shared_ptr<FifoQueue>> queues;
      if (envelope->broadcast) {
        std::shared_lock lock(peer_mtx);

        for (auto& nq : peer_queues) {
          auto node_id = nq.first;
          auto peer_chs = peer_channels[node_id];

          if (peer_chs.contains(ch_id)) {
            queues.push_back(nq.second);
          }
        }
        lock.release();
      } else {
        std::shared_lock lock(peer_mtx);

        auto ok = peer_queues.contains(envelope->to);
        auto contains = false;
        if (ok) {
          auto peer_chs = peer_channels[envelope->to];
          contains = peer_chs.contains(ch_id);
        }
        lock.release();
        if (!ok) {
          // TODO: logger
          // r.logger.Debug("dropping message for unconnected peer", "peer", envelope.To, "channel", chID)
          continue;
        }

        if (!contains) {
          // reactor tried to send a message across a channel that the peer doesn't have available.
          // This is a known issue due to how peer subscriptions work:
          // https://github.com/tendermint/tendermint/issues/6598
          continue;
        }

        queues.push_back(peer_queues[envelope->to]);
      }

      for (auto& q : queues) {
        auto queue_res =
          co_await (q->queue_ch->async_send(boost::system::error_code{}, envelope, asio::use_awaitable) ||
            q->close_ch.async_receive(as_result(asio::use_awaitable)) ||
            done.async_receive(as_result(asio::use_awaitable)));

        if (queue_res.index() == 0) {
        } else if (queue_res.index() == 1) {
          // TODO: logger
          // r.logger.Debug("dropping message for unconnected peer", "peer", envelope.To, "channel", chID)
          co_return std::get<1>(queue_res).error();
        } else if (queue_res.index() == 2) {
          co_return std::get<2>(queue_res).error();
        }
      }
    } else if (res.index() == 1) {
      auto peer_error = std::get<1>(res);
      auto max_peer_capacity = peer_manager->has_max_peer_capacity();
      // TODO: logger
      // r.logger.Error("peer error",
      //   "peer", peerError.NodeID,
      //   "err", peerError.Err,
      //   "disconnecting", peerError.Fatal || maxPeerCapacity,
      // )

      if (peer_error->fatal || max_peer_capacity) {
        // if the error is fatal or all peer slots are in use, we can error (disconnect) from the peer.
        peer_manager->errored(peer_error->node_id, peer_error->err);
      } else {
        // this just decrements the peer score.
        peer_manager->process_peer_event(done, PeerUpdate{.node_id = peer_error->node_id, .status = peer_status_bad});
      }
    } else if (res.index() == 2) {
      co_return std::get<2>(res).error();
    } else {
      co_return err_unreachable;
    }
  }
}

} //namespace tendermint::p2p
