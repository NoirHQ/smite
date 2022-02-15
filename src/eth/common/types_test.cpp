// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <eth/common/transaction.h>

using namespace std;
using namespace eth;

TEST_CASE("[types] transaction", "[eth]") {
  transaction tx = {
    .gas_price = std::numeric_limits<uint256_t>::max(),
    .gas = 2,
  };

  CHECK_THROWS((void)tx.fee());
}
