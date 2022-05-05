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
  boost::asio::io_context context{};

  Channel ch1(context, 1, Chan<EnvelopePtr>{context, 1}, Chan<EnvelopePtr>{context, 1}, Chan<PeerErrorPtr>{context, 1});
  Channel ch2(context, 2, Chan<EnvelopePtr>{context, 1}, Chan<EnvelopePtr>{context, 1}, Chan<PeerErrorPtr>{context, 1});
  Channel ch3(context, 3, Chan<EnvelopePtr>{context, 1}, Chan<EnvelopePtr>{context, 1}, Chan<PeerErrorPtr>{context, 1});

  Chan<Done> done{context};
  auto iter = merged_channel_iterator(context, done, ch1, ch2, ch3);

  co_spawn(
    context,
    [&iter, &done]() -> awaitable<void> {
      while ((co_await iter->next(done)).value()) {
        auto current = iter->envelope();
        std::cout << fmt::format("channel_id: {}, from: {}, to: {}", current->channel_id, current->from, current->to)
                  << std::endl;
      }
    },
    detached);

  co_spawn(
    context,
    [&ch1, &done, &context]() -> awaitable<void> {
      boost::system::error_code ec{};
      steady_timer timer{context};
      for (;;) {
        auto res = co_await (done.async_receive(as_result(use_awaitable)) ||
          ch1.in_ch.async_send(ec,
            std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
            use_awaitable));
        if (res.index() == 0) {
          co_return;
        }
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait(use_awaitable);
      }
    },
    detached);

  co_spawn(
    context,
    [&ch2, &done, &context]() -> awaitable<void> {
      boost::system::error_code ec{};
      steady_timer timer{context};
      for (;;) {
        auto res = co_await (done.async_receive(as_result(use_awaitable)) ||
          ch2.in_ch.async_send(ec,
            std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 2}),
            use_awaitable));
        if (res.index() == 0) {
          co_return;
        }
        timer.expires_after(std::chrono::seconds(2));
        co_await timer.async_wait(use_awaitable);
      }
    },
    detached);

  co_spawn(
    context,
    [&ch3, &done, &context]() -> awaitable<void> {
      boost::system::error_code ec{};
      steady_timer timer{context};
      for (;;) {
        auto res = co_await (done.async_receive(as_result(use_awaitable)) ||
          ch3.in_ch.async_send(ec,
            std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 3}),
            use_awaitable));
        if (res.index() == 0) {
          co_return;
        }
        timer.expires_after(std::chrono::seconds(3));
        co_await timer.async_wait(use_awaitable);
      }
    },
    detached);

  co_spawn(
    context,
    [&context, &done]() -> awaitable<void> {
      steady_timer timer{context};
      timer.expires_after(std::chrono::seconds(5));
      co_await timer.async_wait(use_awaitable);
      done.close();
    },
    detached);
  context.run();
}
