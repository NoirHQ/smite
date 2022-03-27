// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/node/node.h>
#include <appbase/application.hpp>

namespace tendermint {

class NodePlugin : public appbase::plugin<NodePlugin> {
public:
  APPBASE_PLUGIN_REQUIRES();

  NodePlugin(appbase::application&);

  void set_program_options(CLI::App&) override;
  void plugin_initialize(const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

  std::unique_ptr<struct NodePluginImpl> my;

  node::Node& node();
  const node::Node& node() const;
};

} // namespace tendermint
