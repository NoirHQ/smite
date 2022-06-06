// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/indexer/event_sink.h>

namespace noir::consensus::indexer {

struct null_event_sink : public event_sink {

  Result<void> index_block_events(const events::event_data_new_block_header&) override {
    return {};
  }
  Result<void> index_tx_events(const std::vector<tx_result>&) override {
    return {};
  }
  Result<std::vector<int64_t>> search_block_events(std::string query) override {
    return {};
  }
  Result<std::vector<std::shared_ptr<tx_result>>> search_tx_events(std::string query) override {
    return {};
  }
  Result<std::shared_ptr<tx_result>> get_tx_by_hash(bytes hash) override {
    return {};
  }
  Result<bool> has_block(int64_t height) override {
    return {};
  }
  event_sink_type type() override {
    return event_sink_type::null;
  }
  Result<void> stop() override {
    return {};
  }
};

} // namespace noir::consensus::indexer
