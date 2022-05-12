// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/time.h>

namespace noir {

tstamp get_time() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

result<std::time_t> parse_genesis_time(const char* time_str) {
  struct tm tm {};
  if (strptime(time_str, "%Y-%m-%dT%H:%M:%SZ", &tm)) {
    return timegm(&tm);
  } else if (strptime(time_str, "%Y-%m-%dT%H:%M:%S", &tm)) {
    return std::mktime(&tm);
  }

  return make_unexpected("Unknown time format");
}

} // namespace noir
