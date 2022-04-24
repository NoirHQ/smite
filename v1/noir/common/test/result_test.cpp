// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/result.h>
#include <catch2/catch_all.hpp>

using namespace noir;

TEST_CASE("Result", "[noir][common]") {
  SECTION("error propagation") {
    auto foo = []() -> Result<int> {
      return Error("error");
    };

    auto bar = [&]() -> Result<std::string> {
      if (auto ok = foo(); !ok) {
        return ok.error();
      }
      return "ok";
    };

    auto res = bar();
    CHECK(!res);
    CHECK(res.error().message() == "error");
  }
}
