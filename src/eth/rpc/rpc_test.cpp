// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <eth/rpc/rpc.h>

using namespace std;
using namespace eth::rpc;

TEST_CASE("[rpc] check_send_raw_tx", "[params]") {
  fc::variant params;

  SECTION("params") {
    params = fc::variant(1);
    CHECK_THROWS_WITH(rpc::check_send_raw_tx(params), "invalid json request");
    vector<string> v;
    params = fc::variant(v);
    CHECK_THROWS_WITH(rpc::check_send_raw_tx(params), "invalid parameters: not enough params to decode");
    v = {"0x1", "0x2"};
    params = fc::variant(v);
    CHECK_THROWS_WITH(rpc::check_send_raw_tx(params), "too many arguments, want at most 1");
  }

  SECTION("param") {
    vector<uint32_t> v = {0};
    params = fc::variant(v);
    CHECK_THROWS_WITH(rpc::check_send_raw_tx(params), "invalid parameters: json: cannot unmarshal");
  }
}
