// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/indexer/sink/psql/psql.h>
#include <noir/core/codec.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <pqxx/pqxx>

namespace noir::consensus::indexer {

using query_func = std::function<result<void>(pqxx::work&)>;

struct psql_event_sink_impl {

  result<void> index_block_events(const events::event_data_new_block_header& h) {
    return run_in_transaction([this, &h](pqxx::work& tx) -> result<void> {
      auto ts = get_utc_ts();
      std::string query = "INSERT INTO blocks (height, chain_id, created_at) VALUES ($1, $2, $3) ON CONFLICT DO "
                          "NOTHING RETURNING rowid;";
      pqxx::params args;
      args.append(std::to_string(h.header.height));
      args.append(chain_id);
      args.append(ts);
      auto block_id = query_with_id(tx, query, args);
      if (!block_id)
        return make_unexpected(fmt::format("indexing block header: {}", block_id.error()));

      // Insert special block meta-event
      if (auto ok = insert_events(tx, block_id.value(), 0,
            {make_indexed_event(std::string(events::block_height_key), std::to_string(h.header.height))});
          !ok)
        return make_unexpected(fmt::format("block meta-events: {}", ok.error()));
      // Insert all block events
      if (auto ok = insert_events(tx, block_id.value(), 0, h.result_begin_block.events); !ok)
        return make_unexpected(fmt::format("begin-block events: {}", ok.error()));
      if (auto ok = insert_events(tx, block_id.value(), 0, h.result_end_block.events); !ok)
        return make_unexpected(fmt::format("end-block events: {}", ok.error()));
      return {};
    });
  }

  result<void> index_tx_events(const std::vector<tx_result>& txrs) {
    auto ts = get_utc_ts();
    for (const auto& txr : txrs) {
      crypto::sha3_256 hash;
      auto tx_hash = hash(txr.tx);

      auto ok = run_in_transaction([this, &txr, &ts, &tx_hash](pqxx::work& tx) -> result<void> {
        auto block_id = query_with_id(
          tx, "SELECT rowid FROM blocks WHERE height = $1 AND chain_id = $2;", {std::to_string(txr.height), chain_id});
        if (!block_id)
          return make_unexpected(fmt::format("finding block_id: {}", block_id.error()));

        // Insert for this tx_result and capture id for indexing events
        std::string query = "INSERT INTO tx_results (block_id, index, created_at, tx_hash, tx_result) ";
        query.append("VALUES ($1, $2, $3, $4, $5) ON CONFLICT DO NOTHING RETURNING rowid;");
        pqxx::params args;
        args.append(std::to_string(block_id.value()));
        args.append(std::to_string(txr.index));
        args.append(ts);
        args.append(to_hex(tx_hash));
        args.append(to_string(encode(txr)));
        auto tx_id = query_with_id(tx, query, args);
        if (!tx_id)
          return make_unexpected(fmt::format("indexing tx_result: {}", tx_id.error()));

        // Insert special transaction meta-events
        if (auto ok = insert_events(tx, block_id.value(), tx_id.value(),
              {make_indexed_event(std::string(events::tx_hash_key), to_hex(tx_hash)),
                make_indexed_event(std::string(events::tx_height_key), std::to_string(txr.height))});
            !ok)
          return make_unexpected(fmt::format("indexing transaction meta-events: {}", ok.error()));
        // Insert events packaged with transaction
        if (auto ok = insert_events(tx, block_id.value(), tx_id.value(), txr.result.events); !ok)
          return make_unexpected(fmt::format("indexing transaction events: {}", ok.error()));
        return {};
      });
    }
    return {};
  }

  result<void> stop() {
    try {
      C->close();
    } catch (std::exception const& e) {
    }
    return {};
  }

  void setup(const std::string& conn_str, const std::string& new_chain_id) {
    C = std::make_unique<pqxx::connection>(conn_str);
    chain_id = new_chain_id;
  }

private:
  result<void> run_in_transaction(const query_func& f) {
    pqxx::work tx(*C);
    if (auto ok = f(tx); !ok) {
      tx.abort(); // Not actually needed but makes it more explicit
      return make_unexpected(ok.error());
    }
    tx.commit();
    return {};
  }

  result<void> insert_events(pqxx::work& tx, uint32_t block_id, uint32_t tx_id, const std::vector<event>& evts) {
    for (const auto& evt : evts) {
      if (evt.type.empty())
        continue;

      std::string query = "INSERT INTO events (block_id, tx_id, type) VALUES ($1, $2, $3) RETURNING rowid;";
      pqxx::params args;
      args.append(std::to_string(block_id));
      tx_id > 0 ? args.append(std::to_string(tx_id)) : args.append();
      args.append(evt.type);
      auto eid = query_with_id(tx, query, args);
      if (!eid)
        return make_unexpected(eid.error());

      // Add any attributes flagged for indexing
      for (const auto& attr : evt.attributes) {
        try {
          if (!attr.index)
            continue;
          auto composite_key = evt.type + "." + attr.key;
          tx.exec_params("INSERT INTO attributes (event_id, key, composite_key, value) VALUES ($1, $2, $3, $4)",
            eid.value(), attr.key, composite_key, attr.value);
        } catch (std::exception const& e) {
          return make_unexpected(e.what());
        }
      }
    }
    return {};
  }

  result<uint32_t> query_with_id(pqxx::work& tx, const std::string& query, const pqxx::params& args) {
    uint32_t id;
    try {
      pqxx::result r = tx.exec_params(query, args);
      id = r[0][0].as<uint32_t>(); // Assume query always returns one rowid
    } catch (std::exception const& e) {
      return make_unexpected(e.what());
    }
    return id;
  }

  event make_indexed_event(const std::string& composite_key, std::string value) {
    auto pos = composite_key.find('.');
    if (pos == std::string::npos)
      return {composite_key};
    return {composite_key.substr(0, pos), {{composite_key.substr(pos + 1), value, true}}};
  }

  std::string get_utc_ts() const {
    /* use boost to get utc time for now
     * Alternative method is to use gmtime:
         auto now = std::chrono::system_clock::now();
         std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
         std::ostringstream os;
         os << std::put_time(gmtime(&currentTime), "%F %T");
     */
    auto time_utc = boost::posix_time::microsec_clock::universal_time();
    return to_iso_extended_string(time_utc);
  }

  std::unique_ptr<pqxx::connection_base> C{};
  std::string chain_id;
};

psql_event_sink::psql_event_sink(): my(new psql_event_sink_impl()) {}

result<std::shared_ptr<event_sink>> psql_event_sink::new_event_sink(
  const std::string& conn_str, const std::string& chain_id) {
  auto new_sink = std::make_shared<psql_event_sink>();
  try {
    new_sink->my->setup(conn_str, chain_id);
  } catch (std::exception const& e) {
    return make_unexpected(fmt::format("unable to create new_event_sink: {}", e.what()));
  }
  return new_sink;
}

result<void> psql_event_sink::index_block_events(const events::event_data_new_block_header& h) {
  return my->index_block_events(h);
}

result<void> psql_event_sink::index_tx_events(const std::vector<tx_result>& txrs) {
  return my->index_tx_events(txrs);
}

result<void> psql_event_sink::stop() {
  return my->stop();
}

} // namespace noir::consensus::indexer
