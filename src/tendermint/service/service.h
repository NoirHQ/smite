// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/common/common.h>
#include <atomic>

namespace tendermint::service {

const error err_already_started = "already started";
const error err_already_stopped = "already stopped";
const error err_not_started = "not started";

// XXX: optimize atomic operations in memory order - relaxed?
template<typename Derived>
class Service {
public:
  // result<void> on_start() noexcept;
  // void on_stop() noexcept;
  // result<void> on_reset() noexcept;

  result<void> start() noexcept {
    bool started_expected = false;
    if (started.compare_exchange_strong(started_expected, true)) {
      if (stopped.load()) {
        // logger.error("Not starting {} service -- already stopped", name);
        started.store(false);
        return make_unexpected(err_already_stopped);
      }
      // logger.info("Starting {} service", name);
      auto res = static_cast<Derived*>(this)->on_start();
      if (!res) {
        started.store(false);
        return make_unexpected(res.error());
      }
      return {};
    }
    // logger.debug("Not starting {} service -- already started", name);
    return make_unexpected(err_already_started);
  }

  result<void> stop() noexcept {
    bool stopped_expected = false;
    if (stopped.compare_exchange_strong(stopped_expected, true)) {
      if (!started.load()) {
        // logger.error("Not stopping {} service -- has not been started yet", name);
        stopped.store(false);
        return make_unexpected(err_not_started);
      }
      // logger.info("Stopping {} service", name);
      static_cast<Derived*>(this)->on_stop();
      // quit
      return {};
    }
    // logger.debug("Stopping {} service (already stopped)", name);
    return make_unexpected(err_already_stopped);
  }

  result<void> reset() noexcept {
    bool stopped_expected = true;
    if (!stopped.compare_exchange_strong(stopped_expected, false)) {
      // logger.debug("Can't reset {} service, Not stopped", name);
      return make_unexpected(fmt::format("can't reset running {}", name));
    }

    bool started_expected = true;
    started.compare_exchange_strong(started_expected, false);
    return static_cast<Derived*>(this)->on_reset();
  }

  bool is_running() const {
    return started.load() && !stopped.load();
  }

  const std::string& to_string() const {
    return name;
  }

  // set_logger
  // wait
  // quit

protected:
  std::string name;
private:
  std::atomic_bool started;
  std::atomic_bool stopped;
  // quit
};

} // namespace tendermint::service
