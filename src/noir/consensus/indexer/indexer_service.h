//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/overloaded.h>
#include <noir/consensus/indexer/event_sink.h>
#include <noir/consensus/types/event_bus.h>

namespace noir::consensus::indexer {

struct indexer_service {

  explicit indexer_service(std::shared_ptr<event_sink> new_sink, std::shared_ptr<events::event_bus> new_bus)
    : sink(new_sink), bus(new_bus) {}

  result<void> on_start() {
    if (sink->type() == event_sink_type::null)
      return {};
    handle = bus->subscribe("indexer", [&](const events::message msg) {
      std::visit(noir::overloaded{
                   [&](const events::event_data_new_block_header& msg) {
                     sink->index_block_events(msg);
                     required_num_txs = msg.num_txs;
                   },
                   [&](const events::event_data_tx& msg) {
                     txs.emplace_back(msg.tx_result);
                     if (txs.size() == required_num_txs) {
                       sink->index_tx_events(txs);
                       txs.clear();
                       required_num_txs = 0;
                     }
                   },
                   [&](const auto& msg) {},
                 },
        msg.data);
    });
    return {};
  }

  std::shared_ptr<event_sink> sink;
  std::shared_ptr<events::event_bus> bus;
  events::event_bus::subscription handle{};
  std::vector<consensus::tx_result> txs;
  int64_t required_num_txs{};
};

} // namespace noir::consensus::indexer
