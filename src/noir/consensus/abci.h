// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/node.h>
#include <appbase/application.hpp>

namespace noir::consensus {

class abci : public appbase::plugin<abci> {
public:
  APPBASE_PLUGIN_REQUIRES()

  abci() {}
  virtual ~abci() {}

  void set_program_options(CLI::App& app_config) {}

  void plugin_initialize(const CLI::App& app_config) {
    ilog("Initialize abci");
    auto config_ = config_setup();
    config_.base.mode = Validator; // todo - read from config or cli
    node_ = node::new_default_node(std::make_shared<config>(config_));
  }

  void plugin_startup() {
    node_->on_start();
  }
  void plugin_shutdown() {}

  std::unique_ptr<node> node_;
};

} // namespace noir::consensus
