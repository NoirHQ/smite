// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <span>
#include <vector>
#include <string_view>

namespace noir {

/// \addtogroup common
/// \{

/// \brief converts bytes to hex string
/// \param s sequence of bytes
std::string to_hex(std::span<const char> s);

/// \brief converts hex string to bytes
/// \param s hex string
/// \param out output buffer
/// \return size of bytes written
size_t from_hex(std::string_view s, std::span<char> out);

/// \brief converts hex string to bytes
/// \return byte sequence
std::vector<char> from_hex(std::string_view s);

/// \}

} // namespace noir
