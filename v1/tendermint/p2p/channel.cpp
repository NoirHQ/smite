// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/p2p/channel.h>

namespace tendermint::p2p {

auto Channel::send(Chan<Done>& done, EnvelopePtr envelope) -> boost::asio::awaitable<Result<void>> {
  boost::system::error_code ec{};
  auto res = co_await (done.async_receive(as_result(use_awaitable)) || out_ch.async_send(ec, envelope, use_awaitable));
  switch (res.index()) {
  case 0:
    co_return std::get<0>(res).error();
  case 1:
    if (ec) {
      co_return failure(ec);
    }
    co_return success();
  }
}

auto Channel::send_error(Chan<Done>& done, PeerErrorPtr peer_error) -> awaitable<Result<void>> {
  boost::system::error_code ec{};
  auto res =
    co_await (done.async_receive(as_result(use_awaitable)) || err_ch.async_send(ec, peer_error, use_awaitable));
  switch (res.index()) {
  case 0:
    co_return std::get<0>(res).error();
  case 1:
    if (ec) {
      co_return failure(ec);
    }
    co_return success();
  }
}

auto Channel::to_string() -> std::string {
  return fmt::format("p2p.Channel<{:d}:{:s}>", id, name);
}

namespace detail {
  auto iterator_worker(Chan<Done>& done, Channel& ch, Chan<EnvelopePtr>& pipe) -> awaitable<Result<void>> {
    for (;;) {
      auto res = co_await (done.async_receive(as_result(use_awaitable)) || ch.in_ch.async_receive(use_awaitable));
      switch (res.index()) {
      case 0:
        co_return std::get<0>(res).error();
      case 1:
        auto envelope = std::get<1>(res);
        auto res2 = co_await (done.async_receive(as_result(use_awaitable)) ||
          pipe.async_send(boost::system::error_code{}, envelope, use_awaitable));
        if (res2.index() == 0) {
          co_return std::get<0>(res2).error();
        }
      }
    }
  }
} //namespace detail

auto Channel::receive(Chan<Done>& done) -> ChannelIteratorUptr {
  auto iter = std::make_unique<ChannelIterator>(context);
  co_spawn(context, detail::iterator_worker(done, *this, iter->pipe), detached);
  return iter;
}

auto ChannelIterator::next(Chan<Done>& done) -> awaitable<Result<bool>> {
  auto res = co_await (done.async_receive(as_result(use_awaitable)) || pipe.async_receive(as_result(use_awaitable)));
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
}

auto ChannelIterator::envelope() -> EnvelopePtr {
  return current;
}
} // namespace tendermint::p2p
