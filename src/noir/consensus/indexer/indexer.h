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

struct tx_indexer {
  virtual result<void> index(std::vector<std::shared_ptr<tx_result>>) = 0;
  virtual result<std::shared_ptr<tx_result>> get(bytes hash) = 0;
  virtual result<std::vector<std::shared_ptr<tx_result>>> search(std::string query) = 0;
};

struct block_indexer {
  virtual result<bool> has(int64_t height) = 0;
  virtual result<void> index(events::event_data_new_block_header&) = 0;
  virtual result<int64_t> search(std::string query) = 0;
};

} // namespace noir::consensus::indexer
