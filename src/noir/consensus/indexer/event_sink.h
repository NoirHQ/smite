// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/rust.h>
#include <noir/consensus/abci_types.h>
#include <noir/consensus/types/events.h>

namespace noir::consensus::indexer {

enum class event_sink_type {
  null,
  psql
};

struct event_sink {
  virtual result<void> index_block_events(const events::event_data_new_block_header&) = 0;
  virtual result<void> index_tx_events(const std::vector<tx_result>&) = 0;
  virtual result<std::vector<int64_t>> search_block_events(std::string query) = 0;
  virtual result<std::vector<std::shared_ptr<tx_result>>> search_tx_events(std::string query) = 0;
  virtual result<std::shared_ptr<tx_result>> get_tx_by_hash(bytes hash) = 0;
  virtual result<bool> has_block(int64_t height) = 0;
  virtual event_sink_type type() = 0;
  virtual result<void> stop() = 0;
};

} // namespace noir::consensus::indexer
