// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>

namespace noir::crypto {

namespace unsafe {
  uint64_t xxh64(std::span<const char> in);
}

/// \brief generates xxh64 hash
/// \ingroup crypto
struct xxh64 : public hash {
  xxh64();
  ~xxh64();
  hash& init() override;
  hash& update(std::span<const char> in) override;
  void final(std::span<char> out) override;
  uint64_t final();
  size_t digest_size() override;

  uint64_t operator()(std::span<const char> in) {
    init().update(in);
    return final();
  }

private:
  class xxh64_impl;
  std::unique_ptr<xxh64_impl> impl;
};

} // namespace noir::crypto
