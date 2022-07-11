// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/p2p/channel.h>

namespace tendermint::p2p {

auto Channel::send(eo::chan<std::monostate>& done, EnvelopePtr envelope) -> asio::awaitable<Result<void>> {
  auto select = eo::Select{*done, *out_ch << envelope};
  switch (co_await select.index()) {
  case 0:
    co_await select.process<0>();
    co_return err_context_done;
  case 1:
    co_await select.process<1>();
    co_return success();
  }
  co_return err_unreachable;
}

auto Channel::send_error(eo::chan<std::monostate>& done, PeerErrorPtr peer_error) -> asio::awaitable<Result<void>> {
  auto select = eo::Select{*done, *err_ch << peer_error};
  switch (co_await select.index()) {
  case 0:
    co_await select.process<0>();
    co_return err_context_done;
  case 1:
    co_await select.process<1>();
    co_return success();
  }
  co_return err_unreachable;
}

auto Channel::to_string() -> std::string {
  return fmt::format("p2p.Channel<{:d}:{:s}>", id, name);
}

namespace detail {
  auto iterator_worker(eo::chan<std::monostate>& done, Channel& ch, eo::chan<EnvelopePtr>& pipe)
    -> asio::awaitable<void> {
    for (;;) {
      auto select0 = eo::Select{*done, *(*ch.in_ch)};
      switch (co_await select0.index()) {
      case 0:
        co_await select0.process<0>();
        co_return;
      case 1: {
        auto envelope = co_await select0.process<1>();
        auto select1 = eo::Select{*done, pipe << envelope};
        switch (co_await select1.index()) {
        case 0:
          co_await select1.process<0>();
          co_return;
        case 1:
          co_await select1.process<1>();
        }
      }
      }
    }
  }
} // namespace detail

auto Channel::receive(eo::chan<std::monostate>& done) -> ChannelIteratorUptr {
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

auto ChannelIterator::next(eo::chan<std::monostate>& done) -> asio::awaitable<Result<bool>> {
  auto select = eo::Select{*done, *pipe};
  switch (co_await select.index()) {
  case 0:
    co_await select.process<0>();
    current.reset();
    co_return false;
  case 1: {
    auto envelope = co_await select.process<1>();
    if (!envelope) {
      current.reset();
      co_return false;
    }
    current = envelope;
    co_return true;
  }
  }
  co_return err_unreachable;
}

auto ChannelIterator::envelope() -> EnvelopePtr {
  return current;
}
} // namespace tendermint::p2p
