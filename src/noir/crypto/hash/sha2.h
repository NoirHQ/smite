// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>

namespace noir::crypto {

/// \brief generates sha256 hash
/// \ingroup crypto
struct sha256 : public hash {
  sha256();
  ~sha256();
  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  size_t digest_size() override;

private:
  class sha256_impl;
  std::unique_ptr<sha256_impl> impl;
};

} // namespace noir::crypto
