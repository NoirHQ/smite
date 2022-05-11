// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <noir/core/result.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace noir {

class Ticker {
public:
  Ticker(boost::asio::io_context& io_context, std::chrono::seconds duration)
    : io_context(io_context), duration(duration), time_ch(io_context, 1), done_ch(io_context, 1) {}

  [[nodiscard]] static auto create(boost::asio::io_context& io_context, std::chrono::seconds duration)
    -> std::unique_ptr<Ticker> {
    return std::make_unique<Ticker>(io_context, duration);
  }

  void start() {
    co_spawn(io_context, tick_routine(), boost::asio::detached);
  }

  void stop() {
    done_ch.close();
  }

  auto tick_routine() -> boost::asio::awaitable<Result<void>> {
    for (;;) {
      boost::asio::steady_timer t{io_context};
      t.expires_after(duration);
      co_await t.async_wait(boost::asio::use_awaitable);
      boost::system::error_code ec{};
      auto res = co_await (done_ch.async_receive(as_result(boost::asio::use_awaitable)) ||
        time_ch.async_send(ec, std::chrono::system_clock::now(), boost::asio::use_awaitable));
      if (res.index() == 0) {
        co_return std::get<0>(res).error();
      }
      // TODO: how to handle ec?
    }
  }

public:
  Chan<Time> time_ch;

private:
  boost::asio::io_context& io_context;
  Chan<Done> done_ch;
  std::chrono::seconds duration;
};

} //namespace noir