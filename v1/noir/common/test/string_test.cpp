// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/string.h>
#include <catch2/catch_all.hpp>

struct foo {
  std::string to_string() const {
    return "foo";
  }
};

namespace baz {
struct bar {};
std::string to_string(bar b) {
  return "bar";
}
} // namespace baz

TEST_CASE("to_string", "[noir][common]") {
  CHECK(noir::to_string(foo{}) == "foo");
  CHECK(noir::to_string(baz::bar{}) == "bar");
  CHECK(noir::to_string(100) == "100");
}
