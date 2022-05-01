// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/indexer/sink/psql/psql.h>
#include <pqxx/pqxx>
#include <iostream>

using namespace noir::consensus::indexer;

TEST_CASE("psql: postresql local test", "[noir][consensus]") {
  try {
    pqxx::connection C{"postgresql://noir:1234@127.0.0.1/postgres?sslmode=disable"};
    std::cout << "Connected to " << C.dbname() << '\n';
    pqxx::work W{C};
    pqxx::result R{W.exec("SELECT rowid FROM blocks WHERE rowid > '10'")};
    std::cout << "Found " << R.size() << " blocks:\n";
    for (auto row : R)
      std::cout << row[0].c_str() << std::endl;
    // W.exec0("UPDATE employee SET salary = salary*2");
    // W.exec0("INSERT INTO blocks (height, chain_id, created_at) VALUES (999, 'sam', '2022-04-30T14:06:04.491250')");
    W.commit();
  } catch (std::exception const& e) {
    std::cerr << e.what() << '\n';
  }
}

TEST_CASE("psql: psql_event_sink", "[noir][consensus]") {
  auto new_sink =
    psql_event_sink::new_event_sink("postgresql://noir:1234@127.0.0.1/postgres?sslmode=disable", "my_chain");
  if (!new_sink)
    std::cout << new_sink.error() << std::endl;
}
