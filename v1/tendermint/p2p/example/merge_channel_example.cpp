// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <boost/asio/steady_timer.hpp>
#include <tendermint/p2p/channel.h>
#include <iostream>

using noir::Chan;
using noir::Done;
using tendermint::p2p::Channel;
using tendermint::p2p::Envelope;
using tendermint::p2p::EnvelopePtr;
using tendermint::p2p::PeerErrorPtr;
using namespace tendermint;

int main() {
  asio::io_context io_context{};

  Channel ch1(io_context,
    1,
    Chan<EnvelopePtr>{io_context, 1},
    Chan<EnvelopePtr>{io_context, 1},
    Chan<PeerErrorPtr>{io_context, 1});
  Channel ch2(io_context,
    2,
    Chan<EnvelopePtr>{io_context, 1},
    Chan<EnvelopePtr>{io_context, 1},
    Chan<PeerErrorPtr>{io_context, 1});
  Channel ch3(io_context,
    3,
    Chan<EnvelopePtr>{io_context, 1},
    Chan<EnvelopePtr>{io_context, 1},
    Chan<PeerErrorPtr>{io_context, 1});

  Chan<Done> done{io_context};
  auto iter = merged_channel_iterator(io_context, done, ch1, ch2, ch3);

  co_spawn(
    io_context,
    [&iter, &done]() -> asio::awaitable<void> {
      while ((co_await iter->next(done)).value()) {
        auto current = iter->envelope();
        std::cout << fmt::format("channel_id: {}, from: {}, to: {}", current->channel_id, current->from, current->to)
                  << std::endl;
      }
    },
    asio::detached);

  co_spawn(
    io_context,
    [&ch1, &done, &io_context]() -> asio::awaitable<void> {
      system::error_code ec{};
      asio::steady_timer timer{io_context};
      for (;;) {
        auto res = co_await (done.async_receive(as_result(asio::use_awaitable)) ||
          ch1.in_ch.async_send(ec,
            std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
            asio::use_awaitable));
        if (res.index() == 0) {
          co_return;
        }
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait(asio::use_awaitable);
      }
    },
    asio::detached);

  co_spawn(
    io_context,
    [&ch2, &done, &io_context]() -> asio::awaitable<void> {
      system::error_code ec{};
      asio::steady_timer timer{io_context};
      for (;;) {
        auto res = co_await (done.async_receive(as_result(asio::use_awaitable)) ||
          ch2.in_ch.async_send(ec,
            std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 2}),
            asio::use_awaitable));
        if (res.index() == 0) {
          co_return;
        }
        timer.expires_after(std::chrono::seconds(2));
        co_await timer.async_wait(asio::use_awaitable);
      }
    },
    asio::detached);

  co_spawn(
    io_context,
    [&ch3, &done, &io_context]() -> asio::awaitable<void> {
      system::error_code ec{};
      asio::steady_timer timer{io_context};
      for (;;) {
        auto res = co_await (done.async_receive(as_result(asio::use_awaitable)) ||
          ch3.in_ch.async_send(ec,
            std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 3}),
            asio::use_awaitable));
        if (res.index() == 0) {
          co_return;
        }
        timer.expires_after(std::chrono::seconds(3));
        co_await timer.async_wait(asio::use_awaitable);
      }
    },
    asio::detached);

  co_spawn(
    io_context,
    [&io_context, &done]() -> asio::awaitable<void> {
      asio::steady_timer timer{io_context};
      timer.expires_after(std::chrono::seconds(5));
      co_await timer.async_wait(asio::use_awaitable);
      done.close();
    },
    asio::detached);
  io_context.run();
}
