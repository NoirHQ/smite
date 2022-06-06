// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/time.h>
#include <date/date.h>

#include <iomanip>

namespace noir {

tstamp get_time() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

Result<std::time_t> parse_genesis_time(const char* time_str) {
  struct tm tm {};
  if (strptime(time_str, "%Y-%m-%dT%H:%M:%SZ", &tm)) {
    return timegm(&tm);
  } else if (strptime(time_str, "%Y-%m-%dT%H:%M:%S", &tm)) {
    return std::mktime(&tm);
  }

  return Error::format("Unknown time format");
}

std::string tstamp_to_format_str(const tstamp time_stamp) {
  std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> tp{
    std::chrono::seconds{time_stamp / 1000000}};
  std::time_t tt = std::chrono::system_clock::to_time_t(tp);
  std::tm tm = *std::gmtime(&tt); // GMT (UTC)
  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

} // namespace noir
