// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/core/error.h>

using namespace noir;

const Error custom_error = user_error_registry().register_error("custom error");

TEST_CASE("Error", "[noir][core]") {
  SECTION("custom error") {
    CHECK(custom_error.message() == "custom error");
  }

  SECTION("convert to system error") {
    try {
      throw std::system_error(std::error_code(custom_error));
    } catch (const std::exception& e) {
      CHECK(std::string(e.what()) == "custom error");
    }
  }
}
