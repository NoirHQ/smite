// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/config.h>
#include <noir/consensus/indexer/sink/null/null.h>
#include <noir/consensus/indexer/sink/psql/psql.h>

namespace noir::consensus::indexer {

struct sink {
  static std::shared_ptr<event_sink> event_sink_from_config(const std::shared_ptr<config>& cfg) {
    if (cfg->tx_index.indexer == "" || cfg->tx_index.indexer == "null")
      return std::make_shared<null_event_sink>();
    if (cfg->tx_index.indexer == "psql") {
      if (cfg->tx_index.psql_conn.empty()) {
        elog("psql connection settings cannot be empty");
        return nullptr;
      }
      auto ok = psql_event_sink::new_event_sink(cfg->tx_index.psql_conn, cfg->base.chain_id);
      if (!ok) {
        elog(ok.error());
        return nullptr;
      }
      return ok.value();
    }
    elog("unsupported event sink type");
    return nullptr;
  }
};

} // namespace noir::consensus::indexer
