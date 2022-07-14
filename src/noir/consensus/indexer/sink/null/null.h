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
    return success();
  }
  Result<void> index_tx_events(const std::vector<tendermint::abci::TxResult>&) override {
    return success();
  }
  Result<std::vector<int64_t>> search_block_events(std::string query) override {
    return success();
  }
  Result<std::vector<std::shared_ptr<tendermint::abci::TxResult>>> search_tx_events(std::string query) override {
    return success();
  }
  Result<std::shared_ptr<tendermint::abci::TxResult>> get_tx_by_hash(Bytes hash) override {
    return success();
  }
  Result<bool> has_block(int64_t height) override {
    return success();
  }
  event_sink_type type() override {
    return event_sink_type::null;
  }
  Result<void> stop() override {
    return success();
  }
};

} // namespace noir::consensus::indexer
