// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <boost/asio/io_context.hpp>

namespace tendermint::p2p {
// Waker is used to wake up a sleeper when some event occurs. It debounces
// multiple wakeup calls occurring between each sleep, and wakeups are
// non-blocking to avoid having to coordinate goroutines.
class Waker {
public:
  Waker(boost::asio::io_context& io_context): wake_ch(io_context, 1) {}

  auto wake() -> boost::asio::awaitable<void> {
    // A non-blocking send with a size 1 buffer ensures that we never block,
    // and that we queue up at most a single wakeup call between each sleep.
    // TODO: handle default
    co_await wake_ch.async_send(boost::system::error_code{}, {}, boost::asio::use_awaitable);
  }

public:
  noir::Chan<std::monostate> wake_ch;
};
} //namespace tendermint::p2p