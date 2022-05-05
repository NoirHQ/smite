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

struct Envelope {
  std::string from;
  std::string to;
  bool broadcast;
  // Message messgae;
  ChannelId channel_id;
};

using EnvelopePtr = std::shared_ptr<Envelope>;

struct PeerError {
  NodeId node_id;
  Error err;
};

using PeerErrorPtr = std::shared_ptr<PeerError>;

class ChannelIterator {
public:
  explicit ChannelIterator(asio::io_context& io_context): pipe(io_context) {}

  auto next(Chan<Done>& done) -> asio::awaitable<Result<bool>>;
  auto envelope() -> EnvelopePtr;

public:
  Chan<EnvelopePtr> pipe;
  EnvelopePtr current;
};

using ChannelIteratorUptr = std::unique_ptr<ChannelIterator>;

class Channel {
public:
  auto send(Chan<Done>& done, EnvelopePtr envelope) -> boost::asio::awaitable<Result<void>>;
  auto send_error(Chan<Done>& done, PeerErrorPtr peer_error) -> boost::asio::awaitable<Result<void>>;
  auto to_string() -> std::string;
  auto receive(Chan<Done>& done) -> ChannelIteratorUptr;

public:
  Channel(asio::io_context& io_context,
    ChannelId id,
    Chan<EnvelopePtr>&& in_ch,
    Chan<EnvelopePtr>&& out_ch,
    Chan<PeerErrorPtr>&& err_ch)
    : io_context(io_context), id(id), in_ch(std::move(in_ch)), out_ch(std::move(out_ch)), err_ch(std::move(err_ch)) {}

  ChannelId id;
  Chan<EnvelopePtr> in_ch;
  Chan<EnvelopePtr> out_ch;
  Chan<PeerErrorPtr> err_ch;

  // messageType
  std::string name;

  asio::io_context& io_context;
};

namespace detail {
  auto iterator_worker(Chan<Done>& done, Channel& ch, Chan<EnvelopePtr>& pipe) -> asio::awaitable<Result<void>>;

  template<typename T>
  auto merge(asio::io_context& io_context, Chan<Done>& done, Chan<EnvelopePtr>& pipe, T& ch) {
    return iterator_worker(done, ch, pipe);
  }

  template<typename T, typename... Ts>
  auto merge(asio::io_context& io_context, Chan<Done>& done, Chan<EnvelopePtr>& pipe, T& ch, Ts&... chs) {
    return merge(io_context, done, pipe, ch) && merge(io_context, done, pipe, chs...);
  }
} //namespace detail

template<typename... Ts>
auto merged_channel_iterator(asio::io_context& io_context, Chan<Done>& done, Ts&... chs) -> ChannelIteratorUptr {
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
