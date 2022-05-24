// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <noir/net/conn.h>
#include <noir/net/tcp_conn.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>

namespace noir {

const std::size_t default_buf_size = 4096;

template<typename Writer>
class BufferedWriter {
public:
  explicit BufferedWriter(std::shared_ptr<Writer>& writer, std::size_t size = default_buf_size): w(writer), buf(size){};

  auto flush() -> boost::asio::awaitable<Result<void>> {
    if (err)
      co_return err;
    if (n == 0)
      co_return success();

    auto res = co_await w->write(boost::asio::buffer(buf.data(), n));
    if (res.has_error()) {
      err = res.error();
    } else {
      auto transferred = res.value();
      if (transferred < n) {
        err = Error("short write");
        std::copy(buf.begin() + transferred, buf.begin() + n, &buf[0]);
        n -= transferred;
      }
    }
    if (err) {
      co_return err;
    }
    n = 0;
    co_return success();
  }

  void reset(Writer& writer) {
    err.clear();
    buf.clear();
    n = 0;
    w = writer;
  }

  auto available() -> std::size_t {
    return buf.size() - n;
  }

  auto write(Bytes& p) -> boost::asio::awaitable<Result<std::size_t>> {
    std::span<ByteType> bytes_s{p};
    std::size_t total_written{0};
    while (bytes_s.size() > available() && !err) {
      std::size_t remaining = available();
      std::size_t written = 0;
      if (n == 0) {
        auto res = co_await w->write(boost::asio::buffer(bytes_s.data(), remaining));
        if (res.has_error()) {
          err = res.error();
        } else {
          written = res.value();
        }
      } else {
        std::copy(bytes_s.begin(), bytes_s.begin() + remaining, &buf[n]);
        written = remaining;
        n += written;
        co_await flush();
      }
      bytes_s = bytes_s.subspan(written);
      total_written += written;
    }
    if (err)
      co_return err;

    std::copy(bytes_s.begin(), bytes_s.end(), &buf[0]);
    n += bytes_s.size();
    total_written += bytes_s.size();
    co_return success(total_written);
  }

public:
  Bytes buf;
  std::size_t n{0};

private:
  Error err;
  std::shared_ptr<Writer>& w;
};

template <typename Writer>
using BufferedWriterUptr = std::unique_ptr<BufferedWriter<Writer>>;
} //namespace noir
