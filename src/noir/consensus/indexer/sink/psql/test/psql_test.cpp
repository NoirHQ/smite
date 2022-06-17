// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/thread_pool.h>
#include <noir/consensus/common_test.h>
#include <noir/consensus/indexer/indexer_service.h>
#include <noir/consensus/indexer/sink/psql/psql.h>
#include <noir/consensus/types/event_bus.h>

#include <appbase/application.hpp>
#include <pqxx/pqxx>
#include <iostream>

using namespace noir::consensus;
using namespace noir::consensus::indexer;
using namespace std::chrono_literals;

constexpr auto conn_str = "postgresql://noir:1234@127.0.0.1/postgres?sslmode=disable";
constexpr auto chain_id = "test_chain";

bool table_exists(std::string table_name) {
  try {
    pqxx::connection C{conn_str};
    pqxx::work W{C};
    auto row = W.exec1(fmt::format("SELECT EXISTS (SELECT FROM pg_tables WHERE tablename = '{}');", table_name));
    if (row[0].as<bool>())
      return true;
  } catch (std::exception const& e) {
  }
  return false;
}

int64_t get_last_height() {
  try {
    pqxx::connection C{conn_str};
    pqxx::work W{C};
    auto row = W.exec1("SELECT height FROM blocks ORDER BY height DESC LIMIT 1");
    return row[0].as<int64_t>();
  } catch (std::exception const& e) {
  }
  return 0;
}

bool row_exists(int64_t height, std::string my_chain_id) {
  try {
    pqxx::connection C{conn_str};
    pqxx::work W{C};
    auto row = W.exec1(
      fmt::format("SELECT EXISTS (SELECT FROM blocks WHERE height = '{}' AND chain_id = '{}');", height, my_chain_id));
    if (row[0].as<bool>())
      return true;
  } catch (std::exception const& e) {
  }
  return false;
}

TEST_CASE("psql: psql_event_sink", "[noir][consensus]") {
  if (!table_exists("attributes") || !table_exists("blocks") || !table_exists("events") || !table_exists("tx_results"))
    return;
  auto last_height = get_last_height();
  appbase::application app_;
  auto thread = std::make_unique<noir::named_thread_pool>("test_thread", 2);
  auto res = noir::async_thread_pool(thread->get_executor(), [&]() {
    app_.register_plugin<test_plugin>();
    app_.initialize<test_plugin>();
    app_.startup();
    auto ev_bus = std::make_shared<events::event_bus>(app_);
    auto new_sink = psql_event_sink::new_event_sink(conn_str, chain_id);
    if (!new_sink) {
      std::cerr << new_sink.error().message() << std::endl;
      return;
    }
    indexer_service new_service(new_sink.value(), ev_bus);
    new_service.on_start();

    CHECK(ev_bus->has_subscribers() == true);
    CHECK(ev_bus->num_clients() == 1);

    ev_bus->publish("test", events::event_data_new_block_header{.header = {.height = last_height + 1}, .num_txs = 1});
    ev_bus->publish("test", events::event_data_tx{{.height = last_height + 1}});
    app_.exec();
  });

  auto status = res.wait_for(3s);
  app_.quit();
  CHECK(row_exists(last_height + 1, chain_id));
}
