// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/core/result.h>
#include <boost/asio/io_context.hpp>

using namespace noir;

TEST_CASE("Result", "[noir][core]") {
  SECTION("error propagation") {
    auto foo = []() -> Result<int> { return Error("error"); };

    auto bar = [&]() -> Result<std::string> {
      if (auto ok = foo(); !ok) {
        return ok.error();
      }
      return "ok";
    };

    auto res = bar();
    CHECK(!res);
    CHECK(res.error().message() == "error");
  }

  SECTION("custom error propagation") {
    struct CustomError {
      int code;

      operator Error() {
        return Error::format("custom error with code: {}", code);
      }
    };

    auto foo = [](int i) -> Result<void, CustomError> { return CustomError{ .code = i, }; };
    auto bar = [&](int i) -> Result<void> {
      return foo(i).error();
    };

    auto res = bar(1);
    CHECK(!res);
    CHECK(res.error().message() == "custom error with code: 1");
  }

  SECTION("co_spawn") {
    boost::asio::io_context io_context{};
    auto foo = []() -> boost::asio::awaitable<Result<void>> {
      throw std::runtime_error("runtime error");
      co_return success();
    };
    boost::asio::co_spawn(io_context, foo(), [](std::exception_ptr eptr, auto r) {
      CHECK(!r);
      CHECK(!(bool)r.error());
      try {
        std::rethrow_exception(eptr);
      } catch (const std::exception& e) {
        CHECK(std::string(e.what()) == "runtime error");
      }
    });

    io_context.run();
  }
}
