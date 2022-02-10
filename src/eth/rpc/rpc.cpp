// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <eth/rpc/rpc.h>
#include <eth/rpc/tx.h>

namespace eth::rpc {

void rpc::plugin_initialize(const CLI::App& config) {
  ilog("initializing ethereum rpc");
}

void rpc::plugin_startup() {
  ilog("starting ethereum rpc");
}

void rpc::plugin_shutdown() {}

} // namespace eth::rpc
