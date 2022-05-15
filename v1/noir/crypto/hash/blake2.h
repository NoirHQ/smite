// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <blake2.h>
#include <optional>

namespace noir::crypto {

/// \brief generates blake2b_256 hash
/// \ingroup crypto
struct Blake2b256 : public Hash<Blake2b256> {
  using Hash::update;
  using Hash::final;

  auto init() -> Blake2b256&;
  auto update(BytesView in) -> Blake2b256&;
  void final(BytesViewMut out);

  constexpr auto digest_size() const -> size_t {
    return 32;
  }

private:
  std::optional<blake2b_state> state;
};

} // namespace noir::crypto
