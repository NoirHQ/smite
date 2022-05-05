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
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        auto send_res =
          co_await ch.send(done, std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}));
        CHECK(!send_res.has_error());
      },
      detached);

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        auto receive_res = co_await ch.out_ch.async_receive(use_awaitable);
        CHECK(receive_res->from == "alice");
        CHECK(receive_res->to == "bob");
      },
      detached);

    context.run();
  }

  SECTION("SendError") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        auto send_res =
          co_await ch.send_error(done, std::make_shared<p2p::PeerError>(PeerError{"alice", Error{"bob"}}));
        CHECK(!send_res.has_error());
      },
      detached);

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        auto receive_res = co_await ch.err_ch.async_receive(use_awaitable);
        CHECK(receive_res->node_id == "alice");
        CHECK(receive_res->err.message() == "bob");
      },
      detached);

    context.run();
  }

  SECTION("SendWithCanceledContext") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        done.close();
        auto res =
          co_await ch.send(done, std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}));
        CHECK(res.has_error());
      },
      detached);

    context.run();
  }

  SECTION("ReceiveEmptyIteratorBlocks") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    done.close();

    auto res = iter->envelope();
    CHECK((!res ? true : false));
    context.run();
  }

  SECTION("ReceiveWithData") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        co_await ch.in_ch.async_send(boost::system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          use_awaitable);
      },
      detached);

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        CHECK((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK(envelope->from == "alice");
        CHECK(envelope->to == "bob");

        done.close();
      },
      detached);
    context.run();
  }

  SECTION("ReceiveWithCanceledContext") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};
    done.close();

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        CHECK_FALSE((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK((!envelope ? true : false));
      },
      detached);
    context.run();
  }

  SECTION("IteratorWithCanceledContext") {
    boost::asio::io_context context{};

    Channel ch(context, 1, Chan<EnvelopePtr>{context}, Chan<EnvelopePtr>{context}, Chan<PeerErrorPtr>{context});
    Chan<Done> done{context};

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    done.close();

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        CHECK_FALSE((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK((!envelope ? true : false));
      },
      detached);
    context.run();
  }

  SECTION("IteratorCanceledAfterFirstUseBecomesNil") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        co_await ch.in_ch.async_send(boost::system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          use_awaitable);
      },
      detached);

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        CHECK((co_await iter->next(done)).value());

        auto envelope = iter->envelope();
        CHECK(envelope->from == "alice");
        CHECK(envelope->to == "bob");

        done.close();

        CHECK_FALSE((co_await iter->next(done)).value());
        CHECK(!(iter->envelope() ? true : false));
      },
      detached);
    context.run();
  }

  SECTION("IteratorMultipleNextCalls") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        co_await ch.in_ch.async_send(boost::system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          use_awaitable);
      },
      detached);

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        CHECK((co_await iter->next(done)).value());

        auto res = iter->envelope();
        CHECK(res->from == "alice");
        CHECK(res->to == "bob");

        auto res1 = iter->envelope();
        CHECK(res == res1);

        done.close();
      },
      detached);
    context.run();
  }

  SECTION("IteratorProducesNilObjectBeforeNext") {
    boost::asio::io_context context{};

    Channel ch(context,
      1,
      Chan<EnvelopePtr>{context, 1},
      Chan<EnvelopePtr>{context, 1},
      Chan<PeerErrorPtr>{context, 1});
    Chan<Done> done{context};

    auto iter = ch.receive(done);
    CHECK((iter ? true : false));
    CHECK(!(iter->envelope() ? true : false));

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        co_await ch.in_ch.async_send(boost::system::error_code{},
          std::make_shared<Envelope>(Envelope{.from = "alice", .to = "bob", .channel_id = 1}),
          use_awaitable);
      },
      detached);

    co_spawn(
      context,
      [&]() -> awaitable<void> {
        CHECK((iter ? true : false));
        CHECK((co_await iter->next(done)).value());

        auto res = iter->envelope();
        CHECK((res ? true : false));
        CHECK(res->from == "alice");
        CHECK(res->to == "bob");

        done.close();
      },
      detached);
    context.run();
  }
}