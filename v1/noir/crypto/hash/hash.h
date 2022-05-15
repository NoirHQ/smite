// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>

/// \brief crypto namespace
/// \ingroup crypto
namespace noir::crypto {

/// \brief abstracts hash function interface
template<typename Derived>
struct Hash {
  /// \brief calculates and stores the hash value of input data to output buffer
  /// \param in input data
  /// \param out output buffer
  void operator()(ByteSequence auto&& in, ByteSequence auto& out) {
    auto derived = static_cast<Derived*>(this);
    derived->init().update(bytes_view(in)).final(bytes_view_mut(out));
  }

  /// \brief calculates and returns the hash value of input data
  /// \param in input data
  /// \return byte array containing hash
  auto operator()(ByteSequence auto&& in) {
    auto derived = static_cast<Derived*>(this);
    return derived->init().update(bytes_view(in)).final();
  }

  /// \brief updates the hash object with byte array
  auto update(BytesViewConstructible auto&& in) -> Derived& {
    auto derived = static_cast<Derived*>(this);
    return derived->update(bytes_view(in));
  }

  /// \brief stores hash value to output buffer
  void final(BytesViewConstructible auto& out) {
    auto derived = static_cast<Derived*>(this);
    return derived->update(bytes_view_mut(out));
  }

  /// \brief returns hash value
  /// \return byte array containing hash
  auto final() -> Bytes {
    auto derived = static_cast<Derived*>(this);
    Bytes out(derived->digest_size());
    derived->final(out);
    return out;
  }

protected:
  Hash() = default;
};

} // namespace noir::crypto
