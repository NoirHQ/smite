// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/basic_datastream.h>

namespace noir::codec {

const Error err_out_of_range = user_error_registry().register_error("out of range");

} // namespace noir::codec
