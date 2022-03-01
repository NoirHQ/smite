// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/concepts.h>
#include <noir/common/for_each.h>
#include <noir/common/hex.h>
#include <noir/common/types/hash.h>
#include <noir/crypto/hash/xxhash.h>
#include <fmt/format.h>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace noir::jmt {

extern size_t root_nibble_height;

struct nibble {
  constexpr nibble() = default;
  constexpr nibble(const nibble&) = default;
  constexpr nibble(nibble&&) = default;
  constexpr nibble& operator=(const nibble&) = default;

  constexpr nibble(uint8_t v): value(v) {}

  nibble& operator=(uint8_t v) {
    check(v < 16, fmt::format("nibble out of range: {}", v));
    value = v;
    return *this;
  }

  constexpr operator uint8_t() {
    return value;
  }

  std::string to_string() const {
    const char* charmap = "0123456789abcdef";
    return {charmap[value]};
  }

  friend bool operator==(const nibble& a, const nibble& b) {
    return a.value == b.value;
  }

  friend bool operator<(const nibble& a, const nibble& b) {
    return a.value < b.value;
  }

  uint8_t value;
};

struct nibble_path {
  nibble_path() = default;
  nibble_path(nibble_path&&) = default;
  nibble_path(const nibble_path&) = default;
  nibble_path& operator=(const nibble_path& path) = default;

  template<Byte T, size_t N = std::dynamic_extent>
  constexpr nibble_path(std::span<T, N> bytes, bool odd = false): bytes({bytes.begin(), bytes.end()}) {
    check(bytes.size() <= 32);
    std::span s{(uint8_t*)bytes.data(), bytes.size()};
    this->bytes = {s.begin(), s.end()};
    if (!odd) {
      num_nibbles = bytes.size() * 2;
    } else {
      check(!(bytes.back() & 0x0f), "should have odd number of nibbles");
      num_nibbles = bytes.size() * 2 - 1;
    }
  }

  template<Byte T>
  constexpr nibble_path(const std::vector<T>& bytes, bool odd = false): nibble_path(std::span(bytes), odd) {}

  void push(nibble n) {
    check(num_nibbles < root_nibble_height);
    if (num_nibbles % 2 == 0) {
      bytes.push_back(n.value << 4);
    } else {
      bytes[num_nibbles / 2] |= n.value;
    }
    num_nibbles += 1;
  }

  std::optional<nibble> pop() {
    std::optional<nibble> popped;
    if (!num_nibbles)
      return popped;
    if (num_nibbles % 2 == 0) {
      popped = bytes.back() & 0x0f;
      bytes.back() &= 0xf0;
    } else {
      popped = bytes.back() >> 4;
      bytes.pop_back();
    }
    num_nibbles -= 1;
    return popped;
  }

  std::optional<nibble> last() {
    std::optional<nibble> last_nibble;
    if (!num_nibbles)
      return last_nibble;
    if (num_nibbles % 2 == 0) {
      last_nibble = bytes.back() & 0x0f;
    } else {
      last_nibble = bytes.back() >> 4;
    }
    return last_nibble;
  }

  bool get_bit(size_t i) {
    check(i < num_nibbles * 4);
    auto pos = i / 8;
    auto bit = 7 - i % 8;
    return ((bytes[pos] >> bit) & 1) != 0;
  }

  nibble get_nibble(size_t i) {
    check(i < num_nibbles);
    return (bytes[i / 2] >> (i % 2 == 1 ? 0 : 4)) & 0xf;
  }

  nibble get_nibble(size_t i) const {
    check(i < num_nibbles);
    return (bytes[i / 2] >> (i % 2 == 1 ? 0 : 4)) & 0xf;
  }

  // TODO: bits

  struct nibble_iterator {
  private:
    nibble_iterator(const jmt::nibble_path& nibble_path, size_t start, size_t end)
      : nibble_path(nibble_path), pos({start, end}), start(start) {
      check(start <= end);
      check(start <= root_nibble_height);
      check(end <= root_nibble_height);
    }

    friend class jmt::nibble_path;

  public:
    nibble_iterator(const nibble_iterator& it)
      : nibble_path(it.nibble_path), pos({it.pos.start, it.pos.end}), start(it.start) {}

    nibble_iterator& operator=(const nibble_iterator& it) {
      if (this != &it)
        *this = it;
      return *this;
    }

    std::optional<nibble> next() {
      if (pos.start < pos.end) {
        return nibble_path.get_nibble(pos.start++);
      }
      return {};
    }

    std::optional<nibble> peek() {
      if (pos.start < pos.end) {
        return nibble_path.get_nibble(pos.start);
      }
      return {};
    }

    nibble_iterator visited_nibbles() {
      check(start <= pos.start);
      check(pos.start <= root_nibble_height);
      return nibble_iterator(nibble_path, start, pos.start);
    }

    nibble_iterator remaining_nibbles() {
      check(pos.start <= pos.end);
      check(pos.end <= root_nibble_height);
      return nibble_iterator(nibble_path, pos.start, pos.end);
    }

    size_t num_nibbles() const {
      return pos.end - start;
    }

    bool is_finished() const {
      return pos.start == pos.end;
    }

    const jmt::nibble_path& nibble_path;
    struct {
      size_t start;
      size_t end;
    } pos;
    const size_t start;
    std::optional<nibble> item;
  };

  nibble_iterator nibbles() const {
    check(num_nibbles <= root_nibble_height);
    return {*this, 0, num_nibbles};
  }

  bool is_empty() const {
    return num_nibbles == 0;
  }

  std::string to_string() const {
    if (!num_nibbles)
      return "";
    const char* charmap = "0123456789abcdef";
    std::string s;
    auto it = nibbles();
    while (auto n = it.next()) {
      s += charmap[n->value];
    }
    return fmt::format("{}", s);
  }

  friend bool operator==(const nibble_path& a, const nibble_path& b) {
    return std::tie(a.num_nibbles, a.bytes) == std::tie(b.num_nibbles, b.bytes);
  }

  friend bool operator<(const nibble_path& a, const nibble_path& b) {
    return std::tie(a.num_nibbles, a.bytes) < std::tie(b.num_nibbles, b.bytes);
  }

  size_t num_nibbles = 0;
  std::vector<uint8_t> bytes;
};

inline size_t skip_common_prefix(nibble_path::nibble_iterator& x, nibble_path::nibble_iterator& y) {
  size_t count = 0;
  for (;;) {
    if (x.is_finished() || y.is_finished() || x.peek() != y.peek()) {
      break;
    }
    count += 1;
    x.next();
    y.next();
  }
  return count;
}

} // namespace noir::jmt

namespace noir {

template<>
struct hash<jmt::nibble> {
  std::size_t operator()(jmt::nibble n) const {
    return std::hash<decltype(n.value)>{}(n.value);
  }
};

template<>
struct hash<jmt::nibble_path> {
  std::size_t operator()(const jmt::nibble_path& n) const {
    crypto::xxh64 hash;
    hash.init()
      .update({(char*)&n.num_nibbles, sizeof(n.num_nibbles)})
      .update({(const char*)n.bytes.data(), n.bytes.size()});
    return hash.final();
  }
};

} // namespace noir

NOIR_REFLECT(noir::jmt::nibble, value);
