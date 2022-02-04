// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/rpc/jsonrpc.h>
#include <appbase/application.hpp>

namespace eth::rpc {

class rpc : public appbase::plugin<rpc> {
public:
  APPBASE_PLUGIN_REQUIRES((noir::rpc::jsonrpc))
  void set_program_options(CLI::App& config) override {}

  void plugin_initialize(const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();
};

} // namespace eth::rpc
