// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/for_each.h>
#include <fmt/format.h>
#include <compare>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace noir::jmt {

struct nibble {
  constexpr nibble() = default;
  constexpr nibble(const nibble&) = default;
  constexpr nibble(nibble&&) = default;
  constexpr nibble(uint8_t v): value(v) {}

  nibble& operator=(uint8_t v) {
    check(v < 16, fmt::format("nibble out of range: {}", v));
    value = v;
    return *this;
  }

  constexpr operator uint8_t() {
    return value;
  }

  friend bool operator==(const nibble& a, const nibble& b) {
    return a.value == b.value;
  }

  friend bool operator<(const nibble& a, const nibble& b) {
    return a.value < b.value;
  }

  uint8_t value;
};

NOIR_FOR_EACH_FIELD_EMBED(nibble, value);

struct nibble_path {
  nibble_path() = default;
  nibble_path(nibble_path&&) = default;
  nibble_path(const nibble_path&) = default;

  nibble_path& operator=(const nibble_path& path) {
    num_nibbles = path.num_nibbles;
    bytes = path.bytes;
    return *this;
  }

  nibble_path(const std::vector<uint8_t>& bytes, bool odd = false): bytes(bytes) {
    if (!odd) {
      num_nibbles = bytes.size() * 2;
    } else {
      check(!(bytes.back() & 0x0f), "should have odd number of nibbles");
      num_nibbles = bytes.size() * 2 - 1;
    }
  }

  void push(nibble n) {
    if (num_nibbles % 2 == 0) {
      bytes.push_back(n.value);
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

  // TODO: bits

  struct nibble_iterator : std::iterator<std::random_access_iterator_tag, const nibble> {
    nibble_iterator(nibble_path& path, int pos = 0): path(path), pos(pos) {
      if (pos < 0 || pos >= path.num_nibbles) {
        pos = -1;
      } else if (pos < path.num_nibbles) {
        item.emplace(path.get_nibble(pos));
      }
    }
    nibble_iterator(const nibble_iterator& it): nibble_iterator(it.path, it.pos) {}

    nibble_iterator& operator=(const nibble_iterator& it) {
      if (this != &it) {
        path = it.path;
        pos = it.pos;
        item.reset();
        if (pos < 0 || pos >= path.num_nibbles) {
          pos = -1;
        } else if (pos < path.num_nibbles) {
          item.emplace(path.get_nibble(pos));
        }
      }
      return *this;
    }

    friend bool operator==(const nibble_iterator& a, const nibble_iterator& b) {
      return a.pos == b.pos;
    }
    friend bool operator!=(const nibble_iterator& a, const nibble_iterator& b) {
      return a.pos != b.pos;
    }

    const nibble& operator*() const {
      check(item.has_value(), "cannot dereference end iterator");
      return *item;
    }
    const nibble* operator->() const {
      check(item.has_value(), "cannot dereference end iterator");
      return &*item;
    }

    nibble_iterator& operator++() {
      check(item.has_value(), "cannot increment end iterator");
      if (++pos < path.num_nibbles) {
        item.emplace(path.get_nibble(pos));
      } else {
        item = std::nullopt;
        pos = -1;
      }
      return *this;
    }

    nibble_iterator& operator--() {
      if (!item.has_value()) {
        check(path.num_nibbles, "cannot decrement end iterator when the nibble path is empty");
        pos = path.num_nibbles - 1;
      } else {
        check(--pos >= 0, "cannot decrement iterator at the beginning of the nibble path");
      }
      item.emplace(path.get_nibble(pos));
      return *this;
    }

    nibble_iterator operator++(int) {
      nibble_iterator it(*this);
      return ++it;
    }

    nibble_iterator operator--(int) {
      nibble_iterator it(*this);
      return --it;
    }

    nibble_path& path;
    int pos;
    std::optional<nibble> item;
  };

  nibble_iterator begin() {
    return {*this};
  }

  nibble_iterator end() {
    return {*this, -1};
  }

  bool is_empty() {
    return num_nibbles == 0;
  }

  size_t num_nibbles;
  std::vector<uint8_t> bytes;
};

} // namespace noir::jmt

namespace noir {

struct hash {
  std::size_t operator()(jmt::nibble n) const {
    return std::hash<decltype(n.value)>{}(n.value);
  }
};

inline std::string to_string(jmt::nibble n) {
  return std::to_string(n.value);
}

} // namespace noir
