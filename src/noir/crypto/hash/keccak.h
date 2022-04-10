// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <optional>

extern "C" {
#include <KeccakHash.h>
}

namespace noir::crypto {

/// \brief generates keccak256 hash
/// \ingroup crypto
struct keccak256 : public hash<keccak256> {
  using hash::final;

  hash<keccak256>& init();
  hash<keccak256>& update(std::span<const char> in);
  void final(std::span<char> out);
  std::size_t digest_size() const;

private:
  std::optional<Keccak_HashInstance> ctx;
};

} // namespace noir::crypto
