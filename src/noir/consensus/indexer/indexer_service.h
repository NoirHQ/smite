//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/overloaded.h>
#include <noir/consensus/event_bus.h>
#include <noir/consensus/indexer/event_sink.h>

namespace noir::consensus::indexer {

struct indexer_service {

  explicit indexer_service(std::shared_ptr<event_sink> new_sink, std::shared_ptr<events::event_bus> new_bus)
    : sink(new_sink), bus(new_bus) {}

  result<void> on_start() {
    if (sink->type() == event_sink_type::null)
      return {};
    bus->subscribe("indexer", [&](const events::message msg) {
      std::visit(noir::overloaded{
                   [&](const events::event_data_new_block_header& msg) {},
                   [&](const events::event_data_tx& msg) {},
                   [&](const auto& msg) {},
                 },
        msg.data);
    });
    return {};
  }

  std::shared_ptr<event_sink> sink;
  std::shared_ptr<events::event_bus> bus;
};

} // namespace noir::consensus::indexer
