// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/buffered_writer.h>
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_all.hpp>

using namespace noir;

class TestWriter {
public:
  auto write(boost::asio::const_buffer buf) -> boost::asio::awaitable<Result<std::size_t>> {
    co_return buf.size();
  }
};

TEST_CASE("BufferedWriter", "[noir][common]") {
  SECTION("available") {
    std::size_t test_size = 100;
    TestWriter tw{};
    BufferedWriter<TestWriter> bw{tw, test_size};
    CHECK(test_size == bw.available());
  }

  SECTION("writer") {
    std::size_t test_size = 5;
    TestWriter tw{};
    BufferedWriter<TestWriter> bw{tw, test_size};

    boost::asio::io_context io_context{};
    boost::asio::co_spawn(
      io_context,
      [&bw]() -> boost::asio::awaitable<void> {
        Bytes bs = {'\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07'};
        auto res = co_await bw.write(bs);
        CHECK(!res.has_error());
        CHECK(res.value() == bs.size());
        CHECK(bw.buf[0] == '\x05');
        Bytes bs2 = {'\x08', '\x09', '\x0a', '\x0b'};
        auto res2 = co_await bw.write(bs2);
        CHECK(!res2.has_error());
        CHECK(res2.value() == bs2.size());
        CHECK(bw.n == 2);
        CHECK(bw.buf[0] == '\x0a');
      },
      boost::asio::detached);
    io_context.run();
  }

  SECTION("flush") {
    std::size_t test_size = 5;
    TestWriter tw{};
    BufferedWriter<TestWriter> bw{tw, test_size};

    boost::asio::io_context io_context{};
    boost::asio::co_spawn(
      io_context,
      [&]() -> boost::asio::awaitable<void> {
        Bytes bs = {0x0, 0x1, 0x2};
        co_await bw.write(bs);
        CHECK(bw.n == 3);
        co_await bw.flush();
        CHECK(bw.n == 0);
      },
      boost::asio::detached);
    io_context.run();
  }
}