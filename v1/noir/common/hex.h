// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/inttypes.h>
#include <span>
#include <vector>
#include <string_view>

namespace noir {

/// \addtogroup common
/// \{

/// \brief converts bytes to hex string
/// \param s sequence of bytes
std::string to_hex(std::span<const char> s);

std::string to_hex(uint8_t v);
std::string to_hex(uint16_t v);
std::string to_hex(uint32_t v);
std::string to_hex(uint64_t v);
std::string to_hex(uint128_t v);
std::string to_hex(uint256_t v);

/// \brief converts hex string to bytes
/// \param s hex string
/// \param out output buffer
/// \return size of bytes written
size_t from_hex(std::string_view s, std::span<char> out);

void from_hex(std::string_view s, uint8_t& v);
void from_hex(std::string_view s, uint16_t& v);
void from_hex(std::string_view s, uint32_t& v);
void from_hex(std::string_view s, uint64_t& v);
void from_hex(std::string_view s, uint128_t& v);
void from_hex(std::string_view s, uint256_t& v);

/// \brief converts hex string to bytes
/// \return byte sequence
std::vector<char> from_hex(std::string_view s);

/// \}

} // namespace noir
