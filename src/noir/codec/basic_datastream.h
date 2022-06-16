// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/core/core.h>

namespace noir::codec {

extern const Error err_out_of_range;

template<typename T>
class BasicDatastream {
  static_assert(std::is_same_v<std::remove_cv_t<T>, unsigned char>);

public:
  BasicDatastream(std::span<T> buf): buf(buf), pos(buf.begin()) {}
  BasicDatastream(T* data, size_t size): BasicDatastream(std::span(data, size)) {}
  BasicDatastream(BytesViewConstructible auto& buf): BasicDatastream(bytes_view(buf)) {}

  auto skip(size_t s) -> Result<void> {
    if (remaining() < s) {
      return err_out_of_range;
    }
    pos += s;
    return success();
  }

  auto read(ByteSequence auto&& out) -> Result<void> {
    if (remaining() < out.size()) {
      return err_out_of_range;
    }
    std::copy(pos, pos + out.size(), out.begin());
    pos += out.size();
    return success();
  }

  auto read(BytePointer auto data, size_t size) -> Result<void> {
    return read(std::span{byte_pointer_cast(data), size});
  }

  auto reverse_read(ByteSequence auto&& out) -> Result<void> {
    if (remaining() < out.size()) {
      return err_out_of_range;
    }
    std::reverse_copy(pos, pos + out.size(), out.begin());
    pos += out.size();
    return success();
  }

  auto reverse_read(BytePointer auto data, size_t size) -> Result<void> {
    return reverse_read(std::span{byte_pointer_cast(data), size});
  }

  auto peek() -> Result<int> {
    if (remaining() < 1) {
      return err_out_of_range;
    }
    return *pos;
  }

  auto get() -> Result<int> {
    auto res = peek();
    if (res) {
      pos += 1;
    }
    return res;
  }

  auto get(Byte auto& c) -> Result<void> {
    auto res = get();
    if (res) {
      c = res.value();
      return success();
    }
    return res.error();
  }

  auto unget() -> Result<void> {
    if (tellp() < 1) {
      return err_out_of_range;
    }
    pos -= 1;
    return success();
  }

  auto write(ByteSequence auto&& in) -> Result<void> {
    if (remaining() < in.size()) {
      return err_out_of_range;
    }
    std::copy(in.begin(), in.end(), pos);
    pos += in.size();
    return success();
  }

  auto write(const void* data, size_t size) -> Result<void> {
    return write(std::span{reinterpret_cast<const unsigned char*>(data), size});
  }

  auto reverse_write(ByteSequence auto&& in) -> Result<void> {
    if (remaining() < in.size()) {
      return err_out_of_range;
    }
    std::reverse_copy(in.begin(), in.end(), pos);
    pos += in.size();
    return success();
  }

  auto reverse_write(const void* data, size_t size) -> Result<void> {
    return reverse_write(std::span{byte_pointer_cast(data), size});
  }

  auto put(unsigned char c) -> Result<void> {
    if (remaining() < 1) {
      return err_out_of_range;
    }
    *pos++ = c;
    return success();
  }

  auto seekp(size_t p) -> Result<void> {
    if (p >= buf.size()) {
      return err_out_of_range;
    }
    pos = buf.begin() + p;
    return success();
  }

  auto tellp() const -> size_t {
    return std::distance(buf.begin(), pos);
  }

  auto remaining() const -> size_t {
    return std::distance(pos, buf.end());
  }

  auto ptr() -> T* {
    return &*pos;
  }

private:
  std::span<T> buf;
  typename std::span<T>::iterator pos;
};

template<>
class BasicDatastream<size_t> {
public:
  constexpr BasicDatastream(size_t init_size = 0): size(init_size) {}

  constexpr void skip(size_t s) {
    size += s;
  }

  constexpr void write(ByteSequence auto&& in) {
    size += in.size();
  }

  constexpr void write(const void*, size_t s) {
    size += s;
  }

  constexpr void reverse_write(ByteSequence auto&& in) {
    size += in.size();
  }

  constexpr void put(char) {
    size += 1;
  }

  constexpr void seekp(size_t p) {
    size = p;
  }

  constexpr size_t tellp() const {
    return size;
  }

  constexpr size_t remaining() const {
    return 0;
  }

private:
  size_t size;
};

} // namespace noir::codec

#define NOIR_CODEC(CODEC) \
  namespace noir::codec::CODEC { \
    template<typename T> \
    class datastream : public BasicDatastream<T> { \
    public: \
      using BasicDatastream<T>::BasicDatastream; \
    }; \
    template<typename T> \
    constexpr size_t encode_size(const T& v) { \
      datastream<size_t> ds; \
      ds << v; \
      return ds.tellp(); \
    } \
    template<typename T> \
    Bytes encode(const T& v) { \
      Bytes buffer(encode_size(v)); \
      datastream<unsigned char> ds(buffer); \
      ds << v; \
      return buffer; \
    } \
    template<typename T> \
    T decode(std::span<const unsigned char> s) { \
      T v; \
      datastream<const unsigned char> ds(s); \
      ds >> v; \
      return v; \
    } \
  } \
  namespace noir::codec::CODEC
