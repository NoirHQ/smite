// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>

namespace noir::crypto {

/// \brief generates keccak256 hash
/// \ingroup crypto
struct keccak256 : public hash {
  keccak256();
  ~keccak256();
  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class keccak256_impl;
  std::unique_ptr<keccak256_impl> impl;
};

/// \brief generates sha3_256 hash
/// \ingroup crypto
struct sha3_256 : public hash {
  sha3_256();
  ~sha3_256();
  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class sha3_256_impl;
  std::unique_ptr<sha3_256_impl> impl;
};

} // namespace noir::crypto
