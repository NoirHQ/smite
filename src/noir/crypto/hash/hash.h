// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <memory>
#include <span>
#include <vector>

/// \brief crypto namespace
/// \ingroup crypto
namespace noir::crypto {

/// \brief abstracts hash function interface
template<typename Derived>
struct hash {
  /// \brief calculates and stores the hash value of input data to output buffer
  /// \param in input data
  /// \param out output buffer
  void operator()(std::span<const char> in, std::span<char> out) {
    init().update(in).final(out);
  }

  /// \brief calculates and returns the hash value of input data
  /// \param in input data
  /// \return byte array containing hash
  std::vector<char> operator()(std::span<const char> in) {
    return init().update(in).final();
  }

  /// \brief resets hash object context
  hash<Derived>& init() {
    return static_cast<Derived*>(this)->init();
  }

  /// \brief updates the hash object with byte array
  hash<Derived>& update(std::span<const char> in) {
    return static_cast<Derived*>(this)->update(in);
  }

  /// \brief stores hash value to output buffer
  void final(std::span<char> out) {
    static_cast<Derived*>(this)->final(out);
  }

  /// \brief returns hash value
  /// \return byte array containing hash
  std::vector<char> final() {
    std::vector<char> out(digest_size());
    final(out);
    return out;
  }

  /// \brief returns the size of hash value
  std::size_t digest_size() const {
    return static_cast<const Derived*>(this)->digest_size();
  }

protected:
  hash() = default;
};

} // namespace noir::crypto
