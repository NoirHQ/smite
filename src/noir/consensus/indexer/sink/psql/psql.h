// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/indexer/event_sink.h>

namespace noir::consensus::indexer {

struct psql_event_sink : public event_sink {

  psql_event_sink();

  static Result<std::shared_ptr<event_sink>> new_event_sink(const std::string& conn_str, const std::string& chain_id);

  Result<void> index_block_events(const events::event_data_new_block_header& h) override;

  Result<void> index_tx_events(const std::vector<tendermint::abci::TxResult>& txrs) override;

  Result<std::vector<int64_t>> search_block_events(std::string query) override {
    return Error::format("search_block_events is not supported for postgres event_sink");
  }

  Result<std::vector<std::shared_ptr<tendermint::abci::TxResult>>> search_tx_events(std::string query) override {
    return Error::format("search_tx_events is not supported for postgres event_sink");
  }

  Result<std::shared_ptr<tendermint::abci::TxResult>> get_tx_by_hash(Bytes hash) override {
    return Error::format("get_tx_by_hash is not supported for postgres event_sink");
  }

  Result<bool> has_block(int64_t height) override {
    return Error::format("has_block is not supported for postgres event_sink");
  }

  event_sink_type type() override {
    return event_sink_type::psql;
  }

  Result<void> stop() override;

private:
  std::shared_ptr<struct psql_event_sink_impl> my;
};

} // namespace noir::consensus::indexer
