// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/check.h>

using namespace noir;

using Catch::Matchers::Message;

TEST_CASE("[check]", "[common]") {
  // (predicate == true), not throws
  CHECK((check(true, "always success"), true));
  // (predicate == false), throws std::runtime_error with message by default
  CHECK_THROWS_WITH(check(false, "always fail"), "always fail");
  // (predicate == false), throws with the formatted string with arguments
  CHECK_THROWS_WITH(check(false, "int: {}, string: {}, bool: {}", 255, "alice", true), "int: 255, string: alice, bool: true");
  // (predicate == false), throws with the exception type
  CHECK_THROWS_MATCHES(check<std::logic_error>(false, "logic error triggered"), std::logic_error, Message("logic error triggered"));
}
