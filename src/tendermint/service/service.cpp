// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/core/core.h>
#include <tendermint/service/service.h>

namespace noir::service {

const Error err_already_started = user_error_registry().register_error("already started");
const Error err_already_stopped = user_error_registry().register_error("already stopped");
const Error err_not_started = user_error_registry().register_error("not started");

} // namespace noir::service
