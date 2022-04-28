// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <pqxx/pqxx>
#include <iostream>

TEST_CASE("", "[noir][consensus]") {
  try {
    // Connect to the database.
    pqxx::connection C{"dbname=postgres user=noir password=1234 hostaddr=127.0.0.1"};

    std::cout << "Connected to " << C.dbname() << '\n';

    // Start a transaction.
    pqxx::work W{C};

    // Perform a query and retrieve all results.
    pqxx::result R{W.exec("SELECT rowid FROM blocks")};

    // Iterate over results.
    std::cout << "Found " << R.size() << " blocks:\n";
    for (auto row : R)
      std::cout << row[0].c_str() << '\n';

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
