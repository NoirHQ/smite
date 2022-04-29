// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/indexer/event_sink.h>
#include <pqxx/pqxx>

namespace noir::consensus::indexer {

struct psql_event_sink : public event_sink {

  static result<std::shared_ptr<event_sink>> new_event_sink(const std::string& conn_str, const std::string& chain_id) {
    auto new_sink = std::make_shared<psql_event_sink>();
    try {
      new_sink->C = std::make_unique<pqxx::connection>(conn_str);
      new_sink->chain_id = chain_id;
    } catch (std::exception const& e) {
      return make_unexpected(fmt::format("unable to create new_event_sink: {}", e.what()));
    }
    return new_sink;
  }

  result<void> index_block_events(events::event_data_new_block_header&) override {
    return {};
  }

  result<void> index_tx_events(std::vector<std::shared_ptr<tx_result>>) override {
    return {};
  }

  result<std::vector<int64_t>> search_block_events(std::string query) override {
    return make_unexpected("search_block_events is not supported for postgres event_sink");
  }

  result<std::vector<std::shared_ptr<tx_result>>> search_tx_events(std::string query) override {
    return make_unexpected("search_tx_events is not supported for postgres event_sink");
  }

  result<std::shared_ptr<tx_result>> get_tx_by_hash(bytes hash) override {
    return make_unexpected("get_tx_by_hash is not supported for postgres event_sink");
  }

  result<bool> has_block(int64_t height) override {
    return make_unexpected("has_block is not supported for postgres event_sink");
  }

  event_sink_type type() override {
    return event_sink_type::psql;
  }

  result<void> stop() override {
    try {
      C->close();
    } catch (std::exception const& e) {
    }
    return {};
  }

private:
  result<void> run_in_transaction() {}

  result<void> insert_events(uint32_t block_id, uint32_t tx_id, const std::vector<event>& evts) {
    for (const auto& evt : evts) {
      if (evt.type.empty())
        continue;

      std::string query = "INSERT INTO `events` (block_id, tx_id, type) VALUES ($1, $2, $3) RETURNING rowid;";
      std::vector<std::string> args;
      args.push_back(std::to_string(block_id));
      args.push_back(std::to_string(tx_id));
      args.push_back(evt.type);
      auto eid = query_with_id(query, args);
      if (!eid)
        return make_unexpected(eid.error());

      // Add any attributes flagged for indexing
      for (const auto& attr : evt.attributes) {
        try {
          if (!attr.index)
            continue;
          auto composite_key = evt.type + "." + attr.key;
          pqxx::work w(*C);
          w.exec_params("INSERT INTO `attributes` (event_id, key, composite_key, value) VALUES ($1, $2, $3, $4)",
            eid.value(), attr.key, composite_key, attr.value);
        } catch (std::exception const& e) {
          return make_unexpected(e.what());
        }
      }
    }
    return {};
  }

  result<uint32_t> query_with_id(const std::string& query, const std::vector<std::string>& args) {
    uint32_t id;
    try {
      std::string statement = query;
      for (auto pos = 1; pos < args.size(); ++pos) {
        if (pos != 1)
          statement.append(",");
        statement.append("$").append(std::to_string(pos));
      }
      statement.append(")");
      pqxx::work w(*C);
      pqxx::result r = w.exec_params(statement, pqxx::params(args));
      w.commit();
      id = r[0][0].as<uint32_t>();
    } catch (std::exception const& e) {
      return make_unexpected(e.what());
    }
    return id;
  }

  std::unique_ptr<pqxx::connection_base> C{};
  std::string chain_id;
};

} // namespace noir::consensus::indexer
