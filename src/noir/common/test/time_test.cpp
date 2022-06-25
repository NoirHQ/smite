// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/time.h>
#include <date/date.h>

using namespace noir;

TEST_CASE("time: conversion", "[noir][common]") {
  auto now = get_time();
  auto genesis_str = tstamp_to_genesis_str(now); // may lose microsecond precision
  CHECK(genesis_time_to_tstamp(genesis_str));
  CHECK(genesis_time_to_tstamp(genesis_str).value() / 1'000 == now / 1'000);

  auto rfc_format_str = genesis_str.substr(0, genesis_str.length() - 1) + "+00:00";
  CHECK(genesis_time_to_tstamp(genesis_str).value() == genesis_time_to_tstamp(rfc_format_str).value());

  rfc_format_str = genesis_str.substr(0, genesis_str.length() - 1) + "+0000";
  CHECK(genesis_time_to_tstamp(genesis_str).value() == genesis_time_to_tstamp(rfc_format_str).value());

  auto floor_to_second = genesis_str.substr(0, 19);
  CHECK(genesis_time_to_tstamp(floor_to_second)); // read as local time
  CHECK(genesis_time_to_tstamp(floor_to_second + "Z"));
  CHECK(genesis_time_to_tstamp(floor_to_second + "+0000"));
  CHECK(genesis_time_to_tstamp(floor_to_second + "+00:00"));
  CHECK(genesis_time_to_tstamp(floor_to_second + "Z").value() == now / 1'000'000 * 1'000'000);
}
