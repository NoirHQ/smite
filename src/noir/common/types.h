// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types/bytes.h>
#include <noir/common/types/inttypes.h>
#include <fc/io/varint.hpp>

namespace noir {

using ::fc::signed_int;
using ::fc::unsigned_int;

using sender_type = std::string;
using tx_id_type = bytes32;

} // namespace noir
