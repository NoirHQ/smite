// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <chrono>
#include <unordered_map>

namespace noir {

extern const std::unordered_map<std::string_view, std::chrono::nanoseconds> unit_map;

template<typename Duration>
auto parse_duration(std::string_view s) -> Result<Duration> {
  if (s == "0") {
    return Duration{0};
  }

  std::chrono::nanoseconds duration{0};

  auto it = s.begin();
  auto end = s.end();
  auto sign = 1;

  if (*it == '-') {
    sign = -1;
    it += 1;
  }
  while (it != end) {
    auto idx = std::size_t{0};
    auto val = std::stoll(std::string{it, end}, &idx) * sign;
    if (it + idx == end) {
      return Error("no unit");
    }
    if (*(it + idx) != '.') {
      auto uend = std::find_if(it + idx, end, [](auto c) { return std::isdigit(c); });
      auto unit = std::string{it + idx, uend};
      if (unit_map.find(unit) == unit_map.end()) {
        return Error::format("invalid unit: {}", unit);
      }
      duration += val * unit_map.at(unit);
      it = uend;
      continue;
    } else {
      return Error::format("float unsupported yet");
      // idx = 0;
      // auto vald = std::stod(std::string{it, end}, &idx) * sign;
      // auto uend = std::find_if(it + idx, end, [](auto c) { return std::isdigit(c); });
      // auto unit = std::string{it + idx, uend};
      // it = uend;
    }
  }

  return std::chrono::duration_cast<Duration>(duration);
}

} // namespace noir
