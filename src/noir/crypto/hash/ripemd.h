// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>

namespace noir::crypto {

/// \brief generates ripemd160 hash
/// \ingroup crypto
struct ripemd160 : public hash {
  using hash::final;

  ripemd160();
  ~ripemd160();
  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class ripemd160_impl;
  std::unique_ptr<ripemd160_impl> impl;
};

} // namespace noir::crypto
