// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <span>

namespace noir::crypto {

/// \brief generates random bytes
/// \ingroup crypto
void rand_bytes(std::span<char> out);

/// \brief generates random bytes using separated PRNG
/// \ingroup crypto
void rand_priv_bytes(std::span<char> out);

} // namespace noir::crypto
