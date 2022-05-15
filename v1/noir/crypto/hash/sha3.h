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
struct Sha3_256 : public Hash<Sha3_256> {
  using Hash::update;
  using Hash::final;

  auto init() -> Sha3_256&;
  auto update(BytesView in) -> Sha3_256&;
  void final(BytesViewMut out);

  constexpr auto digest_size() const -> size_t {
    return 32;
  }

private:
  std::optional<Keccak_HashInstance> ctx;
};

} // namespace noir::crypto
