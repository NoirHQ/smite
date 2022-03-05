// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <eth/rpc/api.h>

using namespace std;
using namespace eth::api;

TEST_CASE("eth:params: check_params_size", "[eth][api]") {
  fc::variants vs;
  CHECK_THROWS_WITH(api::check_params_size(vs, 1), "missing value for required argument 0");
  vs = fc::variants({"0x1", "test"});
  CHECK_THROWS_WITH(api::check_params_size(vs, 1), "too many arguments, want at most 1");
  vs = fc::variants({"0xf86d018609184e72a000822710940000000000000000000000000000000000000000880de0b6b3a76400000025a0"
                     "feec8647d75ca4d10ebb104e7fa7d5143f6fc9335119f04d630f49dcea495398a02f31173ee22872fe2446c32a2e29"
                     "6787c293d3409a2f9870b2a1e54a9d3f4d40"});
  CHECK_NOTHROW(api::check_params_size(vs, 1));
}

TEST_CASE("eth:params: send_raw_tx", "[eth][api]") {
  fc::variant params;
  api a;

  SECTION("check params fail") {
    params = fc::variant(1);
    CHECK_THROWS_WITH(a.send_raw_tx(params), "invalid json request");
    vector<string> v;
    params = fc::variant(v);
    CHECK_THROWS_WITH(a.send_raw_tx(params), "missing value for required argument 0");
    v = {"0x1", "0x2"};
    params = fc::variant(v);
    CHECK_THROWS_WITH(a.send_raw_tx(params), "too many arguments, want at most 1");
  }

  SECTION("check param fail") {
    vector<uint32_t> v = {0};
    params = fc::variant(v);
    CHECK_THROWS_WITH(a.send_raw_tx(params), "invalid parameters: json: cannot unmarshal");
  }

  SECTION("param success") {
    vector<string> v = {"0xf86d018609184e72a000822710940000000000000000000000000000000000000000880de0b6b3a76400000025a0"
                        "feec8647d75ca4d10ebb104e7fa7d5143f6fc9335119f04d630f49dcea495398a02f31173ee22872fe2446c32a2e29"
                        "6787c293d3409a2f9870b2a1e54a9d3f4d40"};
    params = fc::variant(v);
    CHECK(a.send_raw_tx(params) == "");
  }
}
