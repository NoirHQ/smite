// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/protocol.h>

namespace noir::consensus {

//struct tx {};
using tx = bytes;

using tx_ptr = std::shared_ptr<tx>;

} // namespace noir::consensus
