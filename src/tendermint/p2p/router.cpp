// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/log/log.h>
#include <tendermint/p2p/router.h>
#include <eo/sync.h>
#include <thread>

namespace tendermint::p2p {

using namespace noir;

static auto to_channel_ids(const std::vector<unsigned char>& bytes) -> ChannelIdSet {
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

  if (incoming_connection_window.count() == 0) {
    incoming_connection_window = std::chrono::milliseconds{100};
  }
  if (max_incoming_connection_attempts == 0) {
    max_incoming_connection_attempts = 100;
  }
  return success();
}

auto Router::start(eo::chan<std::monostate>& done) -> asio::awaitable<Result<void>> {
  auto res = co_await transport->listen(endpoint);
  if (!res) {
    co_return res.error();
  }

  co_spawn(io_context, dial_peers(done), asio::detached);
  co_spawn(io_context, evict_peers(done), asio::detached);
  co_spawn(io_context, accept_peers(done, transport), asio::detached);

  co_return success();
}

// dial_peers maintains outbound connections to peers by dialing them.
auto Router::dial_peers(eo::chan<std::monostate>& done) -> asio::awaitable<void> {
  dlog("starting dial routine");
  eo::chan<std::shared_ptr<NodeAddress>> addresses(io_context);
  eo::sync::WaitGroup wg{};

  for (auto i = 0; i < num_concurrent_dials(); i++) {
    wg.add(1);

    // TODO: get executor and run
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
        wg.done();
      },
      asio::detached);
  }

  for (;;) {
    auto dial_res = co_await peer_manager->dial_next(done);
    if (!dial_res && dial_res.error() == err_canceled) {
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

  wg.wait();
}

auto Router::connect_peer(eo::chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
  -> asio::awaitable<void> {
  auto dial_res = co_await dial_peer(done, address);
  if (!dial_res) {
    if (dial_res.error() == err_canceled) {
      co_return;
    }
    dlog("failed to dial peer", "peer", address->node_id, "err", dial_res.error());
    auto dial_failed_res = co_await peer_manager->dial_failed(done, address);
    if (!dial_failed_res) {
      elog("failed to report dial failure", "peer", address->node_id, "err", dial_res.error());
    }
    co_return;
  }

  auto conn = dial_res.value();
  auto handshake_res = co_await handshake_peer(done, conn, address->node_id);
  if (!handshake_res) {
    if (handshake_res.error() == err_canceled) {
      conn->close();
      co_return;
    } else {
      elog("failed to handshake with peer", "peer", address->node_id, "err", handshake_res.error());
      auto dial_failed_res = co_await peer_manager->dial_failed(done, address);
      if (!dial_failed_res) {
        elog("failed to report dial failure", "peer", address->node_id, "err", handshake_res.error());
      }
      conn->close();
      co_return;
    }
  }

  auto peer_info = handshake_res.value();
  auto run_res = co_await run_with_peer_mutex(
    [&]() -> asio::awaitable<Result<void>> { co_return co_await peer_manager->dialed(address); });
  if (!run_res) {
    elog("failed to dial peer", "op", "outgoing/dialing", "peer", address->node_id, "err", run_res.error());
    co_await peer_manager->dial_waker.wake();
    conn->close();
    co_return;
  }

  co_spawn(io_context, route_peer(done, address->node_id, conn, to_channel_ids(peer_info.channels)), asio::detached);
}

auto Router::get_or_make_queue(const NodeId& peer_id, ChannelIdSet& channels) -> std::shared_ptr<FifoQueue> {
  std::scoped_lock _(peer_mtx);

  if (peer_queues.contains(peer_id)) {
    return peer_queues[peer_id];
  }

  // TODO: priority queue
  auto peer_queue = FifoQueue::new_fifo_queue(io_context, queue_buffer_default);
  peer_queues[peer_id] = peer_queue;
  peer_channels[peer_id] = channels;
  return peer_queue;
}

auto Router::dial_peer(eo::chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address)
  -> asio::awaitable<Result<std::shared_ptr<MConnConnection>>> {
  if (options.resolve_timeout.count() > 0) {
    // TODO: resolve timeout
  }

  dlog("resolving peer address", "peer", address->node_id);
  auto resolve_res = address->resolve(done);
  if (!resolve_res) {
    co_return Error::format("failed to resolve address {}: {}", address->to_string(), resolve_res.error());
  }
  auto endpoints = resolve_res.value();
  if (endpoints.empty()) {
    co_return Error::format("address {} did not resolve to any endpoints", address->to_string());
  }

  for (auto& ep : endpoints) {
    if (options.dial_timeout.count() > 0) {
      // TODO: dial timeout
    }

    auto dial_res = co_await transport->dial(ep);
    if (!dial_res) {
      dlog("failed to dial endpoint", "peer", address->node_id, "endpoint", ep, "err", dial_res.error());
    } else {
      dlog("dialed peer", "peer", address->node_id, "endpoint", ep, "err", dial_res.error());
      co_return dial_res.value();
    }
  }
  co_return Error("all endpoints failed");
}

auto Router::num_concurrent_dials() const -> int32_t {
  if (!options.num_current_dials) {
    return std::thread::hardware_concurrency() * 32;
  }
  return options.num_current_dials();
}

auto Router::evict_peers(eo::chan<std::monostate>& done) -> asio::awaitable<void> {
  auto evict_res = co_await peer_manager->evict_next(done);

  if (!evict_res) {
    if (evict_res.error() != err_canceled) {
      elog("failed to find next peer to evict", "err", evict_res.error());
    }
    co_return;
  }
  auto peer_id = evict_res.value();
  ilog("evicting peer", "peer", peer_id);

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
auto Router::accept_peers(eo::chan<std::monostate>& done, std::shared_ptr<MConnTransport>& transport)
  -> boost::asio::awaitable<void> {
  dlog("starting accept routine", "transport", "mconn");
  for (;;) {
    auto accept_res = co_await transport->accept(done);
    if (!accept_res) {
      auto err = accept_res.error();
      if (err == err_canceled || err == err_deadline_exceeded) {
        dlog("stopping accept routine", "transport", "mconn", "err", "context canceled");
        co_return;
      } else if (err == err_eof) {
        dlog("stopping accept routine", "transport", "mconn", "err", "EOF");
        co_return;
      } else {
        elog("failed to accept connection", "transport", "mconn", "err", err);
        continue;
      }
    }

    auto conn = accept_res.value();
    co_spawn(io_context, open_connection(done, conn), asio::detached);
  }
}

auto Router::open_connection(eo::chan<std::monostate>& done, std::shared_ptr<MConnConnection> conn)
  -> asio::awaitable<void> {
  auto re = conn->remote_endpoint();

  auto filter_by_ip_res = filter_peers_ip(done, re);
  if (!filter_by_ip_res) {
    dlog("peer filtered by IP", "ip", re, "err", filter_by_ip_res.error());
    co_return;
  }
  // FIXME: The peer manager may reject the peer during Accepted()
  // after we've handshaked with the peer (to find out which peer it
  // is). However, because the handshake has no ack, the remote peer
  // will think the handshake was successful and start sending us
  // messages.
  //
  // This can cause problems in tests, where a disconnection can cause
  // the local node to immediately redial, while the remote node may
  // not have completed the disconnection yet and therefore reject the
  // reconnection attempt (since it thinks we're still connected from
  // before).
  //
  // The Router should do the handshake and have a final ack/fail
  // message to make sure both ends have accepted the connection, such
  // that it can be coordinated with the peer manager.
  auto handshake_res = co_await handshake_peer(done, conn, "");

  if (!handshake_res) {
    auto handshake_err = handshake_res.error();
    if (handshake_err == err_canceled) {
      co_return;
    } else {
      elog("peer handshake failed", "endpoint", conn->remote_endpoint(), "err", handshake_err);
      co_return;
    }
  }

  auto peer_info = handshake_res.value();
  auto filter_by_id_res = filter_peers_id(done, peer_info.node_id);
  if (!filter_by_id_res) {
    dlog("peer filtered by node ID", "node", peer_info.node_id, "err", filter_by_id_res.error());
    co_return;
  }

  auto run_res = co_await run_with_peer_mutex([&, node_id = peer_info.node_id]() -> asio::awaitable<Result<void>> {
    co_return co_await peer_manager->accepted(node_id);
  });
  if (!run_res) {
    elog("failed to accept connection", "op", "incoming/accepted", "peer", peer_info.node_id, "err", run_res.error());
    co_return;
  }

  co_await route_peer(done, peer_info.node_id, conn, to_channel_ids(peer_info.channels));
}

auto Router::filter_peers_ip(eo::chan<std::monostate>& done, const std::string& endpoint) -> Result<void> {
  if (!options.filter_peer_by_ip) {
    return success();
  }
  return options.filter_peer_by_ip(done, endpoint);
}

auto Router::filter_peers_id(eo::chan<std::monostate>& done, const NodeId& id) -> Result<void> {
  if (!options.filter_peer_by_id) {
    return success();
  }
  return options.filter_peer_by_id(done, id);
}

auto Router::handshake_peer(
  eo::chan<std::monostate>& done, std::shared_ptr<MConnConnection>& conn, const NodeId& expect_id)
  -> asio::awaitable<Result<tendermint::type::NodeInfo>> {
  auto handshake_res = co_await conn->handshake(done, node_info, priv_key);

  if (!handshake_res) {
    co_return handshake_res.error();
  }
  auto [peer_info, peer_key] = handshake_res.value();
  auto validate_res = peer_info.validate();
  if (!validate_res) {
    co_return Error::format("invalid handshake NodeInfo: {}", validate_res.error());
  }

  if (node_id_from_pubkey(*peer_key) == peer_info.node_id) {
    co_return Error::format("peer's public key did not match its node ID {} (expected {})", peer_info.node_id,
      node_id_from_pubkey(*peer_key));
  }
  if (!expect_id.empty() && expect_id != peer_info.node_id) {
    co_return Error::format("expected to connect with peer {}, got {}", expect_id, peer_info.node_id);
  }

  auto comp_res = node_info.compatible_with(peer_info);
  if (!comp_res) {
    auto inactivate_res = peer_manager->inactivate(peer_info.node_id);
    if (!inactivate_res) {
      co_return Error::format("problem inactivating peer {}: {}", peer_info.node_id, inactivate_res.error());
    }
    // TODO: ErrRejected
    co_return Error("rejected");
  }
  co_return peer_info;
}

auto Router::run_with_peer_mutex(std::function<asio::awaitable<Result<void>>(void)>&& fn)
  -> asio::awaitable<Result<void>> {
  std::scoped_lock _(peer_mtx);
  co_return co_await fn();
}

// routePeer routes inbound and outbound messages between a peer and the reactor channels.
// It will close the given connection and send queue when done, or if they are closed elsewhere it will cause this
// method to shut down and return.
auto Router::route_peer(eo::chan<std::monostate>& done,
  const NodeId& peer_id,
  std::shared_ptr<MConnConnection>& conn,
  ChannelIdSet&& channels) -> asio::awaitable<void> {
  co_await peer_manager->ready(done, peer_id, channels);

  auto send_queue = get_or_make_queue(peer_id, channels);
  eo_defer([&](...) {
    std::unique_lock lock(peer_mtx);
    peer_queues.erase(peer_id);
    peer_channels.erase(peer_id);
    lock.release();

    send_queue->close();
  });

  ilog("peer connected", "peer", peer_id, "endpoint", conn->remote_endpoint());
  auto err_ch = std::make_shared<eo::chan<std::shared_ptr<Error>>>(io_context, 2);

  co_spawn(
    io_context,
    [&, err_ch]() -> asio::awaitable<void> {
      auto receive_err = co_await receive_peer(done, peer_id, conn);
      co_await (err_ch->async_send(boost::system::error_code{}, receive_err, asio::use_awaitable) ||
        done.async_receive(as_result(asio::use_awaitable)));
    },
    asio::detached);
  co_spawn(
    io_context,
    [&, err_ch]() -> asio::awaitable<void> {
      auto send_err = co_await send_peer(done, peer_id, conn, send_queue);
      co_await (err_ch->async_send(boost::system::error_code{}, send_err, asio::use_awaitable) ||
        done.async_receive(as_result(asio::use_awaitable)));
    },
    asio::detached);

  auto err = co_await err_ch->async_receive(asio::use_awaitable);
  conn->close();
  send_queue->close();

  err = co_await err_ch->async_receive(asio::use_awaitable);
  if (err || *err == err_eof) {
    ilog("peer disconnected", "peer", peer_id, "endpoint", conn->remote_endpoint());
  } else {
    elog("peer failure", "peer", peer_id, "endpoint", conn->remote_endpoint(), "err", *err);
  }
  co_await peer_manager->disconnected(done, peer_id);
}

// receive_peer receives inbound messages from a peer, deserializes them and passes them on to the appropriate channel.
auto Router::receive_peer(eo::chan<std::monostate>& done, const NodeId& peer_id, std::shared_ptr<MConnConnection> conn)
  -> asio::awaitable<std::shared_ptr<Error>> {
  for (;;) {
    auto receive_res = co_await conn->receive_message(done);
    if (!receive_res) {
      co_return std::make_shared<Error>(receive_res.error());
    }
    auto [ch_id, bz] = receive_res.value();
    std::shared_lock lock(channel_mtx);
    auto contained = channel_queues.contains(ch_id);
    auto queue = channel_queues[ch_id];
    auto message_type = channel_messages[ch_id];
    lock.release();

    if (!contained) {
      dlog("dropping message for unknown channel", "peer", peer_id, "channel", ch_id);
      continue;
    }
    auto msg = message_type->New();
    auto parsed = msg->ParseFromArray(bz->data(), bz->size());
    if (!parsed) {
      elog("message decoding failed, dropping message", "peer", peer_id);
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

    auto queue_res =
      co_await (queue->queue_ch->async_send(boost::system::error_code{},
                  std::make_shared<Envelope>(peer_id, std::shared_ptr<google::protobuf::Message>(msg), ch_id),
                  asio::use_awaitable) ||
        queue->close_ch.async_receive(as_result(asio::use_awaitable)) ||
        done.async_receive(as_result(asio::use_awaitable)));

    if (queue_res.index() == 0) {
      dlog("received message", "peer", peer_id, "message", msg);
    } else if (queue_res.index() == 1) {
      dlog("channel closed, dropping message", "peer", peer_id, "channel", ch_id);
    } else if (queue_res.index() == 2) {
      co_return std::make_shared<Error>(std::get<2>(queue_res).error());
    } else {
      co_return std::make_shared<Error>(err_unreachable);
    }
  }
}

// send_peer sends queued messages to a peer.
auto Router::send_peer(eo::chan<std::monostate>& done,
  const NodeId& peer_id,
  std::shared_ptr<MConnConnection>& conn,
  std::shared_ptr<FifoQueue>& peer_queue) -> asio::awaitable<std::shared_ptr<Error>> {
  for (;;) {
    auto res = co_await (peer_queue->queue_ch->async_receive(asio::use_awaitable) ||
      peer_queue->close_ch.async_receive(as_result(asio::use_awaitable)) ||
      done.async_receive(as_result(asio::use_awaitable)));

    if (res.index() == 0) {
      auto envelope = std::get<0>(res);
      if (!envelope->message) {
        elog("dropping nil message", "peer", peer_id);
        continue;
      }

      auto msg_size = envelope->message->ByteSizeLong();
      auto bz = std::make_shared<std::vector<unsigned char>>(msg_size);
      envelope->message->SerializeToArray(bz->data(), bz->size());

      auto send_res = co_await conn->send_message(done, envelope->channel_id, bz);
      if (!send_res) {
        co_return std::make_shared<Error>(send_res.error());
      }
      dlog("sent message", "peer", envelope->to, "message", envelope->message->GetTypeName());
    } else if (res.index() == 1) {
      co_return nullptr;
    } else if (res.index() == 2) {
      co_return nullptr;
    } else {
      co_return std::make_shared<Error>(err_unreachable);
    }
  }
}

auto Router::open_channel(eo::chan<std::monostate>& done, std::shared_ptr<conn::ChannelDescriptor>& ch_desc)
  -> Result<std::shared_ptr<Channel>> {
  std::unique_lock lock(channel_mtx);

  if (channel_queues.contains(ch_desc->id)) {
    return Error::format("channel {} already exists", ch_desc->id);
  }
  ch_descs.push_back(ch_desc);
  // TODO: priority queue
  auto queue = FifoQueue::new_fifo_queue(io_context, ch_desc->recv_buffer_capacity);
  auto out_ch = std::make_shared<eo::chan<EnvelopePtr>>(io_context, ch_desc->recv_buffer_capacity);
  auto err_ch = std::make_shared<eo::chan<PeerErrorPtr>>(io_context, ch_desc->recv_buffer_capacity);

  auto channel = Channel::new_channel(io_context, ch_desc->id, queue->queue_ch, out_ch, err_ch);
  channel->name = ch_desc->name;

  channel_queues[ch_desc->id] = queue;
  channel_messages[ch_desc->id] = ch_desc->message_type;

  node_info.add_channels(ch_desc->id);
  transport->add_channel_descriptor(std::vector<std::shared_ptr<conn::ChannelDescriptor>>{ch_desc});

  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      eo_defer([&](...) {
        std::unique_lock lock(channel_mtx);
        channel_queues.erase(ch_desc->id);
        channel_messages.erase(ch_desc->id);
        lock.release();
        queue->close();
      });
      co_await route_channel(done, ch_desc->id, out_ch, err_ch, ch_desc->message_type);
    },
    asio::detached);

  return channel;
}

// route_channel receives outbound channel messages and routes them to the appropriate peer.
// It also receives peer errors and reports them to the peer manager.
// It returns when either the outbound channel or error channel is closed, or the Router is stopped.
// wrapper is an optional message wrapper for messages, see Wrapper for details.
auto Router::route_channel(eo::chan<std::monostate>& done,
  ChannelId ch_id,
  std::shared_ptr<eo::chan<EnvelopePtr>> out_ch,
  std::shared_ptr<eo::chan<PeerErrorPtr>> err_ch,
  std::shared_ptr<google::protobuf::Message> wrapper) -> asio::awaitable<void> {
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
      if (envelope->message->GetTypeName() != wrapper->GetTypeName()) {
        elog("failed to wrap message", "channel", ch_id);
        continue;
      }

      std::vector<std::shared_ptr<FifoQueue>> queues{};
      if (envelope->broadcast) {
        std::shared_lock lock(peer_mtx);

        for (auto& [node_id, q] : peer_queues) {
          auto peer_chs = peer_channels[node_id];

          if (peer_chs.contains(ch_id)) {
            queues.push_back(q);
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
          dlog("dropping message for unconnected peer", "peer", envelope->to, "channel", ch_id);
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
          dlog("dropping message for unconnected peer", "peer", envelope->to, "channel", ch_id);
        } else if (queue_res.index() == 2) {
          co_return;
        }
      }
    } else if (res.index() == 1) {
      auto peer_error = std::get<1>(res);
      auto max_peer_capacity = peer_manager->has_max_peer_capacity();
      elog("peer error", "peer", peer_error->node_id, "err", peer_error->err, "disconnecting",
        peer_error->fatal || max_peer_capacity);

      if (peer_error->fatal || max_peer_capacity) {
        // if the error is fatal or all peer slots are in use, we can error (disconnect) from the peer.
        co_await peer_manager->errored(peer_error->node_id, peer_error->err);
      } else {
        // this just decrements the peer score.
        peer_manager->process_peer_event(done, std::make_shared<PeerUpdate>(peer_error->node_id, peer_status_bad));
      }
    } else if (res.index() == 2) {
      co_return;
    } else {
      co_return;
    }
  }
}

} // namespace tendermint::p2p
