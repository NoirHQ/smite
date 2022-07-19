// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <string>

namespace noir::config {

struct TxIndexConfig {
  std::string indexer;
  std::string psql_conn;

  TxIndexConfig() {
    indexer = "null";
    psql_conn = "";
  }
};

} // namespace noir::config
