// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/p2p/channel.h>

namespace tendermint::p2p {

auto Channel::send(Chan<Done>& done, EnvelopePtr envelope) -> asio::awaitable<Result<void>> {
  system::error_code ec{};
  auto res = co_await (
    done.async_receive(as_result(asio::use_awaitable)) || out_ch.async_send(ec, envelope, asio::use_awaitable));
  switch (res.index()) {
  case 0:
    co_return std::get<0>(res).error();
  case 1:
    if (ec) {
      co_return failure(ec);
    }
    co_return success();
  }
  co_return err_unreachable;
}

auto Channel::send_error(Chan<Done>& done, PeerErrorPtr peer_error) -> asio::awaitable<Result<void>> {
  system::error_code ec{};
  auto res = co_await (
    done.async_receive(as_result(asio::use_awaitable)) || err_ch.async_send(ec, peer_error, asio::use_awaitable));
  switch (res.index()) {
  case 0:
    co_return std::get<0>(res).error();
  case 1:
    if (ec) {
      co_return failure(ec);
    }
    co_return success();
  }
  co_return err_unreachable;
}

auto Channel::to_string() -> std::string {
  return fmt::format("p2p.Channel<{:d}:{:s}>", id, name);
}

namespace detail {
  auto iterator_worker(Chan<Done>& done, Channel& ch, Chan<EnvelopePtr>& pipe) -> asio::awaitable<Result<void>> {
    for (;;) {
      auto in_res =
        co_await (done.async_receive(as_result(asio::use_awaitable)) || ch.in_ch.async_receive(asio::use_awaitable));
      switch (in_res.index()) {
      case 0:
        co_return std::get<0>(in_res).error();
      case 1:
        auto envelope = std::get<1>(in_res);
        auto pipe_res = co_await (done.async_receive(as_result(asio::use_awaitable)) ||
          pipe.async_send(system::error_code{}, envelope, asio::use_awaitable));
        if (pipe_res.index() == 0) {
          co_return std::get<0>(pipe_res).error();
        }
      }
    }
  }
} //namespace detail

auto Channel::receive(Chan<Done>& done) -> ChannelIteratorUptr {
  auto iter = std::make_unique<ChannelIterator>(io_context);
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      co_await detail::iterator_worker(done, *this, iter->pipe);
      iter->pipe.close();
    },
    asio::detached);
  return iter;
}

auto ChannelIterator::next(Chan<Done>& done) -> asio::awaitable<Result<bool>> {
  auto res =
    co_await (done.async_receive(as_result(asio::use_awaitable)) || pipe.async_receive(as_result(asio::use_awaitable)));
  switch (res.index()) {
  case 0:
    current.reset();
    co_return false;
  case 1:
    auto envelope = std::get<1>(res);
    if (!envelope) {
      current.reset();
      co_return false;
    }
    current = envelope.value();
    co_return true;
  }
  co_return err_unreachable;
}

auto ChannelIterator::envelope() -> EnvelopePtr {
  return current;
}
} // namespace tendermint::p2p
