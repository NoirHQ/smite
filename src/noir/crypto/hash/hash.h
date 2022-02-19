// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <memory>
#include <span>
#include <vector>

/// \brief crypto namespace
/// \ingroup crypto
namespace noir::crypto {

/// \brief abstracts hash function interface
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
  virtual hash& init() = 0;

  /// \brief updates the hash object with byte array
  virtual hash& update(std::span<const char> in) = 0;

  /// \brief stores hash value to output buffer
  virtual void final(std::span<char> out) = 0;

  /// \brief returns hash value
  /// \return byte array containing hash
  std::vector<char> final() {
    std::vector<char> out(digest_size());
    final(out);
    return out;
  }

  /// \brief returns the size of hash value
  virtual size_t digest_size() = 0;
};

} // namespace noir::crypto

#define NOIR_CRYPTO_HASH(name) \
  namespace noir::crypto { \
    namespace unsafe { \
      void name(std::span<const char> in, std::span<char> out); \
    } \
    name::name(): impl(new name##_impl) {} \
    name::~name() = default; \
    hash& name::init() { \
      impl->init(); \
      return *this; \
    } \
    hash& name::update(std::span<const char> in) { \
      impl->update(in); \
      return *this; \
    } \
    void name::final(std::span<char> out) { \
      check(digest_size() <= out.size(), "insufficient output buffer"); \
      impl->final(out); \
    } \
    size_t name::digest_size() { \
      return impl->digest_size(); \
    } \
  }
