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

  Channel ch(context, 1, Chan<EnvelopePtr>{context}, Chan<EnvelopePtr>{context}, Chan<PeerErrorPtr>{context});

  Chan<Done> done{context};

  auto iter = ch.receive(done);

  co_spawn(
    context,
    [&iter, &done]() -> awaitable<void> {
      while ((co_await iter->next(done)).value()) {
        auto current = iter->envelope();
        std::cout << current->channel_id << std::endl;
      }
    },
    detached);

  co_spawn(
    context,
    [&ch, &done, &context]() -> awaitable<void> {
      boost::system::error_code ec{};
      steady_timer timer{context};
      for (;;) {
        auto res = co_await (done.async_receive(as_result(use_awaitable)) ||
          ch.in_ch.async_send(ec, std::make_shared<Envelope>(Envelope{1}), use_awaitable));
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
    [&context, &done]() -> awaitable<void> {
      steady_timer timer{context};
      timer.expires_after(std::chrono::seconds(5));
      co_await timer.async_wait(use_awaitable);
      done.close();
    },
    detached);
  context.run();
}
