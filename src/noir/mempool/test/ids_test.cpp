// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <noir/mempool/ids.h>

using namespace noir;
using namespace noir::mempool;

TEST_CASE("MempoolIDsBasic", "[noir][mempool]") {
  auto ids = MempoolIDs{};

  auto peer_id = consensus::node_id{"0011223344556677889900112233445566778899"};
  // no error check

  ids.reserve_for_peer(peer_id);
  CHECK(1 == ids.get_for_peer(peer_id));
  ids.reclaim(peer_id);

  ids.reserve_for_peer(peer_id);
  CHECK(2 == ids.get_for_peer(peer_id));
  ids.reclaim(peer_id);
}
