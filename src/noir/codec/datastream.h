// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/basic_datastream.h>

namespace noir::codec {

template<typename T>
using datastream = basic_datastream<T>;

} // namespace noir::codec
