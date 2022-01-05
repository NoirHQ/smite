// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <appbase/application.hpp>

namespace noir::tendermint {

class tendermint : public appbase::plugin<tendermint> {
public:
  APPBASE_PLUGIN_REQUIRES()

  void set_program_options(CLI::App& cli, CLI::App& config) override;

  void plugin_initialize(const CLI::App& cli, const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();
};

} // namespace noir::tendermint

namespace noir::tendermint::config {

bool load();
bool save();
void set(const char* key, const char* value);

} // namespace noir::tendermint::config

namespace noir::tendermint::log {

void info(const char* msg);
void debug(const char* msg);
void error(const char* msg);

} // namespace noir::tendermint::log

namespace noir::tendermint::node {

void start();
void stop();

} // namespace noir::tendermint::node

