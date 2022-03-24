// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/rpc/resource.h>

using namespace noir::rpc;

TEST_CASE("resource: single query", "[noir][rpc][resource]") {
  auto res = "test?foo=bar";
  resource r(res);

  CHECK(r.path == "test");
  CHECK(r.full_query == "foo=bar");
  CHECK(r.query["foo"] == "bar");
}

TEST_CASE("resource: multi query", "[noir][rpc][resource]") {
  auto res = "test?foo=bar&age=35&query='height=1000'&=temp";
  resource r(res);

  CHECK(r.path == "test");
  CHECK(r.full_query == "foo=bar&age=35&query='height=1000'&=temp");
  CHECK(r.query["foo"] == "bar");
  CHECK(r.query["age"] == "35");
  CHECK(r.query["query"] == "'height=1000'");
  CHECK(r.query[""] == "temp");
}

TEST_CASE("resource: no query", "[noir][rpc][resource]") {
  auto res = "test";
  resource r(res);
  CHECK(r.path == "test");
}

TEST_CASE("resource: empty query", "[noir][rpc][resource]") {
  auto res = "test?";
  resource r(res);
  CHECK(r.path == "test");
}

TEST_CASE("resource: parsing failed", "[noir][rpc][resource]") {
  auto res = "test?foo";
  CHECK_THROWS(resource(res));

  res = "test?foo&bar";
  CHECK_THROWS(resource(res));

  res = "test?&bar";
  CHECK_THROWS(resource(res));
}
