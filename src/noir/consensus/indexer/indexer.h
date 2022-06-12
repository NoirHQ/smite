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

struct tx_indexer {
  virtual Result<void> index(std::vector<std::shared_ptr<tx_result>>) = 0;
  virtual Result<std::shared_ptr<tx_result>> get(Bytes hash) = 0;
  virtual Result<std::vector<std::shared_ptr<tx_result>>> search(std::string query) = 0;
};

struct block_indexer {
  virtual Result<bool> has(int64_t height) = 0;
  virtual Result<void> index(events::event_data_new_block_header&) = 0;
  virtual Result<int64_t> search(std::string query) = 0;
};

} // namespace noir::consensus::indexer
