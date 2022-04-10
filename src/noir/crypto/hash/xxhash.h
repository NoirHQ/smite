// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <xxhash.h>

namespace noir::crypto {

/// \brief generates xxh64 hash
/// \ingroup crypto
struct xxh64 : public hash<xxh64> {
  ~xxh64();
  hash<xxh64>& init();
  hash<xxh64>& update(std::span<const char> in);
  void final(std::span<char> out);
  uint64_t final();
  std::size_t digest_size() const;

  uint64_t operator()(std::span<const char> in) {
    init().update(in);
    return final();
  }

private:
  XXH64_state_t* state = nullptr;
};

} // namespace noir::crypto
