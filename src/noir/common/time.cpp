// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/time.h>
#include <date/date.h>
#include <date/tz.h>

#include <sstream>

namespace noir {

tstamp get_time() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

std::string tstamp_to_localtime_str(tstamp t) {
  // Note : should be used only to approximate time because we'll lose decimal places for seconds
  std::time_t tt = t / 1'000'000; // convert from microseconds to seconds
  std::tm tm = *std::localtime(&tt);
  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}
std::string tstamp_to_str(tstamp t) {
  return date::format("%F %T %Z", date::sys_time<std::chrono::microseconds>(std::chrono::microseconds(t)));
}
std::string tstamp_to_genesis_str(tstamp t) {
  // Convert from microseconds to milliseconds
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(t));
  return date::format("%FT%TZ", date::sys_time<std::chrono::milliseconds>(ms));
}

Result<tstamp> genesis_time_to_tstamp(const std::string_view& genesis_time) {
  if (genesis_time.length() < 19)
    return Error::format("not enough length: {}", genesis_time);
  std::istringstream ss((std::string(genesis_time)));
  date::sys_time<std::chrono::microseconds> tp;
  if (genesis_time.ends_with("Z")) {
    // Check Zulu timezone : 2022-06-22T09:36:50.123456Z
    ss >> date::parse("%FT%T%Z", tp);
    if (!ss.fail())
      return tp.time_since_epoch().count();
  } else if (genesis_time.find('+', 19) != std::string::npos || genesis_time.find('-', 19) != std::string::npos) {
    // Check RFC3339 format : 2022-06-22T09:36:50.123456+0000
    ss >> date::parse("%FT%T%z", tp);
    if (!ss.fail())
      return tp.time_since_epoch().count();
  } else {
    // Check local timezone : 2022-06-22T17:36:50.123456
    date::local_time<std::chrono::microseconds> local_tp;
    ss >> date::parse("%FT%T", local_tp);
    if (!ss.fail()) {
      auto zoned_t = make_zoned(date::current_zone(), local_tp);
      return zoned_t.get_sys_time().time_since_epoch().count();
    }
  }
  return Error::format("unable to parse - not supported format: {}", genesis_time);
}

} // namespace noir
