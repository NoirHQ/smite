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

  CHECK(r.path_ == "test");
  CHECK(r.full_query_ == "foo=bar");
  CHECK(r.query_["foo"] == "bar");
}

TEST_CASE("resource: multi query", "[noir][rpc][resource]") {
  auto res = "test?foo=bar&age=35&query='height=1000'&=temp";
  resource r(res);

  CHECK(r.path_ == "test");
  CHECK(r.full_query_ == "foo=bar&age=35&query='height=1000'&=temp");
  CHECK(r.query_["foo"] == "bar");
  CHECK(r.query_["age"] == "35");
  CHECK(r.query_["query"] == "'height=1000'");
  CHECK(r.query_[""] == "temp");
}

TEST_CASE("resource: no query", "[noir][rpc][resource]") {
  auto res = "test";
  resource r(res);
  CHECK(r.path_ == "test");
}

TEST_CASE("resource: empty query", "[noir][rpc][resource]") {
  auto res = "test?";
  resource r(res);
  CHECK(r.path_ == "test");
}

TEST_CASE("resource: parsing failed", "[noir][rpc][resource]") {
  auto res = "test?foo";
  CHECK_THROWS(resource(res));

  res = "test?foo&bar";
  CHECK_THROWS(resource(res));

  res = "test?&bar";
  CHECK_THROWS(resource(res));
}
