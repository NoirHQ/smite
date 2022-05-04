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
  // std::string from;
  // std::string to;
  // bool broadcast;
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
  explicit ChannelIterator(io_context& context): pipe(context) {}

  auto next(Chan<Done>& done) -> awaitable<Result<bool>>;
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
  Channel(io_context& context,
    ChannelId id,
    Chan<EnvelopePtr>&& in_ch,
    Chan<EnvelopePtr>&& out_ch,
    Chan<PeerErrorPtr>&& err_ch)
    : context(context), id(id), in_ch(std::move(in_ch)), out_ch(std::move(out_ch)), err_ch(std::move(err_ch)) {}

  ChannelId id;
  Chan<EnvelopePtr> in_ch;
  Chan<EnvelopePtr> out_ch;
  Chan<PeerErrorPtr> err_ch;

  // messageType
  std::string name;

  io_context& context;
};

namespace detail {
  auto iterator_worker(Chan<Done>& done, Channel& ch, Chan<EnvelopePtr>& pipe) -> awaitable<Result<void>>;

  template<typename T>
  auto merge(io_context& context, Chan<Done>& done, Chan<EnvelopePtr>& pipe, T& ch) {
    return iterator_worker(done, ch, pipe);
  }

  template<typename T, typename... Ts>
  auto merge(io_context& context, Chan<Done>& done, Chan<EnvelopePtr>& pipe, T& ch, Ts&... chs) {
    return merge(context, done, pipe, ch) && merge(context, done, pipe, chs...);
  }
} //namespace detail

template<typename... Ts>
auto merged_channel_iterator(io_context& context, Chan<Done>& done, Ts&... chs) -> ChannelIteratorUptr {
  auto iter = std::make_unique<ChannelIterator>(context);

  co_spawn(
    context,
    [&]() -> awaitable<void> {
      co_await (done.async_receive(as_result(use_awaitable)) || detail::merge(context, done, iter->pipe, chs...));
    },
    detached);

  return iter;
}

} // namespace tendermint::p2p
