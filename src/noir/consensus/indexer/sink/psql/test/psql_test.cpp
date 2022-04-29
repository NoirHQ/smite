// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/indexer/sink/psql/psql.h>
#include <iostream>

using namespace noir::consensus::indexer;

TEST_CASE("psql: postresql local test", "[noir][consensus]") {
  try {
    // Connect to the database.
    // pqxx::connection C{"dbname=postgres user=noir password=1234 hostaddr=127.0.0.1"};
    pqxx::connection C{"postgresql://noir:1234@127.0.0.1/postgres?sslmode=disable"};

    std::cout << "Connected to " << C.dbname() << '\n';

    // Start a transaction.
    pqxx::work W{C};

    // Perform a query and retrieve all results.
    pqxx::result R{W.exec("SELECT rowid FROM blocks WHERE rowid > '10'")};

    // Iterate over results.
    std::cout << "Found " << R.size() << " blocks:\n";
    for (auto row : R)
      std::cout << row[0].c_str() << std::endl;

    // Perform a query and check that it returns no result.
    //    std::cout << "Doubling all employees' salaries...\n";
    //    W.exec0("UPDATE employee SET salary = salary*2");

    // Commit the transaction.
    std::cout << "Making changes definite: ";
    W.commit();
    std::cout << "OK.\n";
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
