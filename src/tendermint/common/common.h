// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/rust.h>

namespace tendermint {

using namespace noir;

} // namespace tendermint

#define errorf(FORMAT, ...) \
  noir::make_unexpected(fmt::format(FORMAT, __VA_ARGS__))
