// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <mutex>
#include <vector>

namespace noir::consensus {

struct bit_array {
  std::mutex mtx;
  int bits{};
  std::vector<bool> elem;

  bit_array() = default;
  bit_array(const bit_array& b): bits(b.bits), elem(b.elem) {}

  static std::shared_ptr<bit_array> new_bit_array(int bits_) {
    auto ret = std::make_shared<bit_array>();
    ret->bits = bits_;
    ret->elem.resize(bits_);
    return ret;
  }

  int size() const {
    return bits;
  }

  bool get_index(int i) {
    std::lock_guard<std::mutex> g(mtx);
    if (i >= bits)
      return false;
    return elem[i];
  }

  bool set_index(int i, bool v) {
    std::lock_guard<std::mutex> g(mtx);
    if (i >= bits)
      return false;
    elem[i] = v;
    return true;
  }

  std::shared_ptr<bit_array> copy() const {
    auto copy = std::make_shared<bit_array>();
    copy->bits = bits;
    copy->elem = elem;
    return copy;
  }

  std::string string() const {
    std::string ret{};
    for (auto e : elem) {
      if (e)
        ret.append("x");
      else
        ret.append("_");
    }
    return ret;
  }

  bytes get_bytes() const {
    auto num_bytes = (bits + 7) / 8;
    bytes bs;
    bs.reserve(num_bytes);
    for (auto i = 0; i < bits; i += 8) {
      uint8_t byte_{};
      for (auto j = 0; j < 8 && (j + i) < bits; j++) {
        if (elem[j + i]) {
          byte_ |= 1 << (7 - j);
        } else {
          byte_ &= ~(1 << (7 - j));
        }
      }
      bs.push_back(static_cast<char>(byte_));
    }
    return bs;
  }

  template<typename T>
  inline friend T& operator<<(T& ds, const bit_array& v) {
    ds << v.bits;
    bytes bs = v.get_bytes();
    ds << bs;
    return ds;
  }

  template<typename T>
  inline friend T& operator>>(T& ds, bit_array& v) {
    ds >> v.bits;

    auto num_bytes = (v.bits + 7) / 8;
    v.elem.resize(v.bits);
    bytes bs;
    ds >> bs;
    int i{0};
    for (auto byte_ : bs) {
      for (auto j = 0; j < 8; j++) {
        if ((byte_ >> (7 - j)) & 1)
          v.set_index(i, true);
        else
          v.set_index(i, false);
        i++;
      }
    }

    return ds;
  }
};

} // namespace noir::consensus

template<>
struct noir::is_foreachable<noir::consensus::bit_array> : std::false_type {};
