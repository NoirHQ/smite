// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/types.h>
#include <noir/eth/common/transaction.h>

using namespace std;
using namespace noir;
using namespace noir::eth;

TEST_CASE("eth:types: transaction", "[eth][common]") {
  transaction tx = {
    .gas_price = std::numeric_limits<uint256_t>::max(),
    .gas = 2,
  };

  CHECK_THROWS((void)tx.fee());
}
