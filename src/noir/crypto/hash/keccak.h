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
struct Keccak256 : public Hash<Keccak256> {
  using Hash::final;
  using Hash::update;

  auto init() -> Keccak256&;
  auto update(std::span<const unsigned char> in) -> Keccak256&;
  void final(std::span<unsigned char> out);

  constexpr auto digest_size() const -> size_t {
    return 32;
  }

private:
  std::optional<Keccak_HashInstance> ctx;
};

} // namespace noir::crypto
