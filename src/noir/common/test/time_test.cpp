// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/time.h>
#include <vector>

using namespace noir;

TEST_CASE("time", "[noir][common]") {
  std::vector<const char*> time_str({"2022-03-23T13:23:00Z", "2022-03-23T22:23:00", "112:42"});
  std::vector<const char*> expected_str({"1648041780", "1648041780", ""});

  for (auto i = 0; i < time_str.size(); i++) {
    auto res = parse_genesis_time(time_str[i]);
    if (res) {
      CHECK(std::to_string(res.value()).compare(expected_str[i]) == 0);
    }
  }
}