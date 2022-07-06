// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/core/core.h>
#include <tendermint/p2p/types.h>
#include <tendermint/types/node_id.h>
#include <cstdint>
#include <memory>

namespace tendermint::p2p {

using namespace noir;

class Envelope {
public:
  Envelope(const NodeId& peer_id, const std::shared_ptr<google::protobuf::Message>& msg, const ChannelId& ch_id)
    : from(peer_id), message(msg), channel_id(ch_id) {}

public:
  std::string from;
  std::string to;
  bool broadcast;
  std::shared_ptr<google::protobuf::Message> message;
  ChannelId channel_id;
};

using EnvelopePtr = std::shared_ptr<Envelope>;

struct PeerError {
  NodeId node_id;
  Error err;
  bool fatal;
};

using PeerErrorPtr = std::shared_ptr<PeerError>;

class ChannelIterator {
public:
  explicit ChannelIterator(asio::io_context& io_context): pipe(io_context) {}

  auto next(Chan<std::monostate>& done) -> asio::awaitable<Result<bool>>;
  auto envelope() -> EnvelopePtr;

public:
  Chan<EnvelopePtr> pipe;
  EnvelopePtr current;
};

using ChannelIteratorUptr = std::unique_ptr<ChannelIterator>;

class Channel {
public:
  static auto new_channel(asio::io_context& io_context,
    ChannelId id,
    const std::shared_ptr<Chan<EnvelopePtr>>& in_ch,
    const std::shared_ptr<Chan<EnvelopePtr>>& out_ch,
    const std::shared_ptr<Chan<PeerErrorPtr>>& err_ch) -> std::shared_ptr<Channel> {
    return std::shared_ptr<Channel>(new Channel(io_context, id, in_ch, out_ch, err_ch));
  }
  auto send(Chan<std::monostate>& done, EnvelopePtr envelope) -> boost::asio::awaitable<Result<void>>;
  auto send_error(Chan<std::monostate>& done, PeerErrorPtr peer_error) -> boost::asio::awaitable<Result<void>>;
  auto to_string() -> std::string;
  auto receive(Chan<std::monostate>& done) -> ChannelIteratorUptr;

public:
  ChannelId id;
  std::shared_ptr<Chan<EnvelopePtr>> in_ch;
  std::shared_ptr<Chan<EnvelopePtr>> out_ch;
  std::shared_ptr<Chan<PeerErrorPtr>> err_ch;

  // messageType
  std::string name;

  asio::io_context& io_context;

private:
  Channel(asio::io_context& io_context,
    ChannelId id,
    const std::shared_ptr<Chan<EnvelopePtr>>& in_ch,
    const std::shared_ptr<Chan<EnvelopePtr>>& out_ch,
    const std::shared_ptr<Chan<PeerErrorPtr>>& err_ch)
    : io_context(io_context), id(id), in_ch(in_ch), out_ch(out_ch), err_ch(err_ch) {}
};

using ChannelIdSet = std::set<ChannelId>;

namespace detail {
  auto iterator_worker(Chan<std::monostate>& done, Channel& ch, Chan<EnvelopePtr>& pipe)
    -> asio::awaitable<Result<void>>;

  template<typename T>
  auto merge(asio::io_context& io_context, Chan<std::monostate>& done, Chan<EnvelopePtr>& pipe, T& ch) {
    return iterator_worker(done, ch, pipe);
  }

  template<typename T, typename... Ts>
  auto merge(asio::io_context& io_context, Chan<std::monostate>& done, Chan<EnvelopePtr>& pipe, T& ch, Ts&... chs) {
    return merge(io_context, done, pipe, ch) && merge(io_context, done, pipe, chs...);
  }
} //namespace detail

template<typename... Ts>
auto merged_channel_iterator(asio::io_context& io_context, Chan<std::monostate>& done, const Ts&... chs)
  -> ChannelIteratorUptr {
  auto iter = std::make_unique<ChannelIterator>(io_context);

  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      co_await (
        done.async_receive(as_result(asio::use_awaitable)) || detail::merge(io_context, done, iter->pipe, chs...));
      iter->pipe.close();
    },
    asio::detached);

  return iter;
}

} // namespace tendermint::p2p
