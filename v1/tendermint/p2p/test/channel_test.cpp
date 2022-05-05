// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <tendermint/core/core.h>
#include <tendermint/p2p/channel.h>
#include <iostream>

using namespace tendermint;
using noir::Error;
using tendermint::p2p::Channel;
using tendermint::p2p::Envelope;
using tendermint::p2p::EnvelopePtr;
using tendermint::p2p::PeerError;
using tendermint::p2p::PeerErrorPtr;

TEST_CASE("Channel", "[tendermint][p2p]") {
  SECTION("Send") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        auto send_res =
          co_await ch.send(done, std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}));
        CHECK(!send_res.has_error());
      },
      asio::detached);

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        auto receive_res = co_await ch.out_ch.async_receive(asio::use_awaitable);
        CHECK(receive_res->from == "alice");
        CHECK(receive_res->to == "bob");
      },
      asio::detached);

    io_context.run();
  }

  SECTION("SendError") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        auto send_res =
          co_await ch.send_error(done, std::make_shared<p2p::PeerError>(PeerError{"alice", Error{"bob"}}));
        CHECK(!send_res.has_error());
      },
      asio::detached);

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        auto receive_res = co_await ch.err_ch.async_receive(asio::use_awaitable);
        CHECK(receive_res->node_id == "alice");
        CHECK(receive_res->err.message() == "bob");
      },
      asio::detached);

    io_context.run();
  }

  SECTION("SendWithCanceledio_context") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        done.close();
        auto res =
          co_await ch.send(done, std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}));
        CHECK(res.has_error());
      },
      asio::detached);

    io_context.run();
  }

  SECTION("ReceiveEmptyIteratorBlocks") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    done.close();

    auto res = iter->envelope();
    CHECK((!res ? true : false));
    io_context.run();
  }

  SECTION("ReceiveWithData") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        co_await ch.in_ch.async_send(system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          asio::use_awaitable);
      },
      asio::detached);

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        CHECK((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK(envelope->from == "alice");
        CHECK(envelope->to == "bob");

        done.close();
      },
      asio::detached);
    io_context.run();
  }

  SECTION("ReceiveWithCanceledio_context") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};
    done.close();

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        CHECK_FALSE((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK((!envelope ? true : false));
      },
      asio::detached);
    io_context.run();
  }

  SECTION("IteratorWithCanceledio_context") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context},
      Chan<EnvelopePtr>{io_context},
      Chan<PeerErrorPtr>{io_context});
    Chan<Done> done{io_context};

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    done.close();

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        CHECK_FALSE((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK((!envelope ? true : false));
      },
      asio::detached);
    io_context.run();
  }

  SECTION("IteratorCanceledAfterFirstUseBecomesNil") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        co_await ch.in_ch.async_send(system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          asio::use_awaitable);
      },
      asio::detached);

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        CHECK((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK(envelope->from == "alice");
        CHECK(envelope->to == "bob");

        done.close();

        CHECK_FALSE((co_await iter->next(done)).value());
        CHECK(!(iter->envelope() ? true : false));
      },
      asio::detached);
    io_context.run();
  }

  SECTION("IteratorMultipleNextCalls") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        co_await ch.in_ch.async_send(system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          asio::use_awaitable);
      },
      asio::detached);

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        CHECK((co_await iter->next(done)).value());

        auto res = iter->envelope();
        CHECK(res->from == "alice");
        CHECK(res->to == "bob");

        auto res1 = iter->envelope();
        CHECK(res == res1);

        done.close();
      },
      asio::detached);
    io_context.run();
  }

  SECTION("IteratorProducesNilObjectBeforeNext") {
    asio::io_context io_context{};

    Channel ch(io_context,
      1,
      Chan<EnvelopePtr>{io_context, 1},
      Chan<EnvelopePtr>{io_context, 1},
      Chan<PeerErrorPtr>{io_context, 1});
    Chan<Done> done{io_context};

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));
    CHECK(!(iter->envelope() ? true : false));

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        co_await ch.in_ch.async_send(system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          asio::use_awaitable);
      },
      asio::detached);

    co_spawn(
      io_context,
      [&]() -> asio::awaitable<void> {
        CHECK((iter ? true : false));
        CHECK((co_await iter->next(done)).value());

        auto res = iter->envelope();
        CHECK((res ? true : false));
        CHECK(res->from == "alice");
        CHECK(res->to == "bob");

        done.close();
      },
      asio::detached);
    io_context.run();
  }
}