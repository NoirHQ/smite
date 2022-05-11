// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/chrono.h>
#include <unordered_map>

namespace noir {

auto parse_duration(std::string_view s) -> Result<std::chrono::steady_clock::duration> {
  static std::unordered_map<std::string_view, std::chrono::steady_clock::duration> unit_map = {
    {"ns", std::chrono::nanoseconds(1)},
    {"us", std::chrono::microseconds(1)},
    {"ms", std::chrono::milliseconds(1)},
    {"s", std::chrono::seconds(1)},
    {"m", std::chrono::minutes(1)},
    {"h", std::chrono::hours(1)},
  };

  std::chrono::steady_clock::duration duration{};

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
      duration += val * unit_map[unit];
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

  return duration;
}

} // namespace noir
