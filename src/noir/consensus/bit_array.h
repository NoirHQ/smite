// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/common/for_each.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <random>
#include <vector>

namespace noir::consensus {

struct bit_array : public std::enable_shared_from_this<bit_array> {
  int bits{};
  std::vector<bool> elem;
  std::mutex mtx;

  bit_array() = default;
  bit_array(const bit_array& b): bits(b.bits), elem(b.elem) {}

  static std::shared_ptr<bit_array> new_bit_array(int bits_) {
    auto ret = std::make_shared<bit_array>();
    ret->bits = bits_;
    ret->elem.resize(bits_);
    return ret;
  }

  static std::shared_ptr<bit_array> copy(const std::shared_ptr<bit_array>& other) {
    if (other == nullptr) {
      return nullptr;
    }
    return new_bit_array(other->bits);
  }

  int size() const {
    if (this == nullptr) { ///< NOT a very nice way of coding; need to refactor later
      return 0;
    }
    return bits;
  }

  bool get_index(int i) {
    if (this == nullptr) { ///< NOT a very nice way of coding; need to refactor later
      return false;
    }
    std::scoped_lock g(mtx);
    if (i >= bits)
      return false;
    return elem[i];
  }

  bool set_index(int i, bool v) {
    if (this == nullptr) { ///< NOT a very nice way of coding; need to refactor later
      return false;
    }
    std::scoped_lock g(mtx);
    if (i >= bits)
      return false;
    elem[i] = v;
    return true;
  }

  std::shared_ptr<bit_array> update(std::shared_ptr<bit_array> o) {
    if (o == nullptr)
      return nullptr;
    std::scoped_lock<std::mutex, std::mutex> g(mtx, o->mtx);
    elem = o->elem;
    return shared_from_this();
  }

  std::shared_ptr<bit_array> sub(std::shared_ptr<bit_array> o) {
    if (this == nullptr || o == nullptr) ///< NOT a very nice way of coding; need to refactor later
      return nullptr;
    std::scoped_lock<std::mutex, std::mutex> g(mtx, o->mtx);
    auto c = copy_bits(bits);
    auto smaller = std::min(elem.size(), o->elem.size());
    for (auto i = 0; i < smaller; i++) {
      c->elem[i] = c->elem[i] & (~o->elem[i]); // and not
    }
    return c; // TODO: check if sub is correctly implemented
  }

  /// \brief returns a bit_array resulting from a bitwise OR of two bit_arrays
  std::shared_ptr<bit_array> or_op(std::shared_ptr<bit_array> o) {
    if (o == nullptr)
      return copy();
    std::scoped_lock<std::mutex, std::mutex> g(mtx, o->mtx);
    auto c = copy_bits(std::max(bits, o->bits));
    auto smaller = std::min(elem.size(), o->elem.size());
    for (auto i = 0; i < smaller; i++) {
      c->elem[i] = c->elem[i] | (o->elem[i]); // or
    }
    return c;
  }

  std::shared_ptr<bit_array> not_op() {
    if (this == nullptr) ///< NOT a very nice way of coding; need to refactor later
      return nullptr;
    std::scoped_lock g(mtx);
    auto c = copy();
    for (auto i = 0; i < c->elem.size(); i++)
      c->elem[i] = ~(c->elem[i]);
    return c;
  }

  std::shared_ptr<bit_array> copy() const {
    if (this == nullptr) ///< NOT a very nice way of coding; need to refactor later
      return nullptr;
    auto copy = std::make_shared<bit_array>();
    copy->bits = bits;
    copy->elem = elem;
    return copy;
  }

  std::shared_ptr<bit_array> copy_bits(int bits) const {
    if (bits > elem.size())
      bits = elem.size();
    auto ret = std::make_shared<bit_array>();
    ret->bits = bits;
    ret->elem.resize(bits);
    std::copy(elem.begin(), elem.begin() + bits, ret->elem.begin());
    return ret;
  }

  int num_elems(const int bits) const {
    return (bits + 63) / 64;
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

  Bytes get_bytes() const {
    auto num_bytes = (bits + 7) / 8;
    Bytes bs;
    bs.raw().reserve(num_bytes);
    for (auto i = 0; i < bits; i += 8) {
      uint8_t byte_{};
      for (auto j = 0; j < 8 && (j + i) < bits; j++) {
        if (elem[j + i]) {
          byte_ |= 1 << (7 - j);
        } else {
          byte_ &= ~(1 << (7 - j));
        }
      }
      bs.raw().push_back(static_cast<unsigned char>(byte_));
    }
    return bs;
  }

  /// \brief returns a random index for a bit. If there is no value, returns 0, false
  std::tuple<int, bool> pick_random() {
    if (this == nullptr || elem.empty())
      return {0, false};
    std::scoped_lock g(mtx);
    auto true_indices = get_true_indices();
    if (true_indices.empty())
      return {0, false};
    std::vector<int> result;
    std::sample(
      true_indices.begin(), true_indices.end(), std::back_inserter(result), 1, std::mt19937{std::random_device{}()});
    return {result[0], true};
  }

  std::vector<int> get_true_indices() {
    std::vector<int> ret;
    for (auto i = 0; i < elem.size(); i++) {
      if (elem[i])
        ret.push_back(i);
    }
    return ret;
  }

  template<typename T>
  inline friend T& operator<<(T& ds, const bit_array& v) {
    ds << v.bits;
    Bytes bs = v.get_bytes();
    ds << bs;
    return ds;
  }

  template<typename T>
  inline friend T& operator>>(T& ds, bit_array& v) {
    ds >> v.bits;

    auto num_bytes = (v.bits + 7) / 8;
    v.elem.resize(v.bits);
    Bytes bs(num_bytes);
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
struct noir::IsForeachable<noir::consensus::bit_array> : std::false_type {};
