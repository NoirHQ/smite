// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/concepts.h>
#include <noir/common/types/string.h>
#include <vector>

namespace noir {

/// \brief alias name of default byte type
/// \ingroup common
using byte_type = char;

/// \brief alias name for variable-length byte sequence
/// \ingroup common
template<Byte T>
using bytes_vec = std::vector<T>;

/// \brief convenient name for variable-length sequence of default byte type
/// \ingroup common
using bytes = bytes_vec<byte_type>;

/// \brief stringify bytes
/// \ingroup common
std::string to_string(const bytes_vec<char>& v);

/// \brief stringify bytes
/// \ingroup common
std::string to_string(const bytes_vec<unsigned char>& v);

} // namespace noir
