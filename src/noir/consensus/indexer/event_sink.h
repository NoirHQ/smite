// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>
#include <noir/consensus/types/events.h>
#include <noir/core/result.h>

namespace noir::consensus::indexer {

enum class event_sink_type {
  null,
  psql
};

struct event_sink {
  virtual Result<void> index_block_events(const events::event_data_new_block_header&) = 0;
  virtual Result<void> index_tx_events(const std::vector<tendermint::abci::TxResult>&) = 0;
  virtual Result<std::vector<int64_t>> search_block_events(std::string query) = 0;
  virtual Result<std::vector<std::shared_ptr<tx_result>>> search_tx_events(std::string query) = 0;
  virtual Result<std::shared_ptr<tx_result>> get_tx_by_hash(Bytes hash) = 0;
  virtual Result<bool> has_block(int64_t height) = 0;
  virtual event_sink_type type() = 0;
  virtual Result<void> stop() = 0;
};

} // namespace noir::consensus::indexer
