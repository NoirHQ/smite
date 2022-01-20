// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/tx_pool/unapplied_tx_queue.hpp>
#include <iostream>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;

TEST_CASE("add_tx", "[tx_pool]") {
  unapplied_tx_queue tx_queue;

  tx tx[10];

  uint64_t gas[10] = { 100, 200, 500, 800, 1500, 7000, 300, 400, 500, 500};
  uint64_t nonce[10] = { 9, 5, 6, 7, 8, 1, 2, 3, 10, 5};

  for (uint64_t i = 0; i < 10; i++) {
    tx[i] = {"user", tx_id_type{std::to_string(i)}, gas[i], nonce[i]};
    tx_queue.add_tx(std::make_shared<struct tx>(std::move(tx[i])));
  }

  CHECK(tx_queue.size() == 9);

}
