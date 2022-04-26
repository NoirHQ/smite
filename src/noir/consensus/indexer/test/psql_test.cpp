// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <pgxx/pgxx>

TEST_CASE("", "[noir][consensus]") {
  pqxx::connection c{"postgresql://accounting@localhost/company"};
  pqxx::work txn{c};
}
