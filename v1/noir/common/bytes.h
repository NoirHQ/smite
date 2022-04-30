// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/concepts.h>
#include <noir/common/string.h>
#include <vector>

namespace noir {

/// \brief alias name of default byte type
/// \ingroup common
using ByteType = char;

/// \brief alias name for variable-length byte sequence
/// \ingroup common
template<Byte T>
using BytesVec = std::vector<T>;

/// \brief convenient name for variable-length sequence of default byte type
/// \ingroup common
using Bytes = BytesVec<ByteType>;

/// \brief stringify bytes
/// \ingroup common
std::string to_string(const BytesVec<char>& v);

/// \brief stringify bytes
/// \ingroup common
std::string to_string(const BytesVec<unsigned char>& v);

} // namespace noir
