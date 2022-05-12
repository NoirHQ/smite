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
  auto current = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
  auto current_tt = std::chrono::system_clock::to_time_t(current);
  auto tm = gmtime(&current_tt);
  auto local_tt = std::mktime(tm);
  char utc[40];
  char local[40];
  strftime(utc, 40, "%Y-%m-%dT%H:%M:%SZ", tm);
  strftime(local, 40, "%Y-%m-%dT%H:%M:%S", tm);

  std::vector<const char*> time_str({utc, local, "112:42"});
  std::vector<const char*> expected_str({std::to_string(current_tt).c_str(), std::to_string(local_tt).c_str(), ""});

  for (auto i = 0; i < time_str.size(); i++) {
    auto res = parse_genesis_time(time_str[i]);
    if (res) {
      CHECK(std::to_string(res.value()).compare(expected_str[i]) == 0);
    }
  }
}