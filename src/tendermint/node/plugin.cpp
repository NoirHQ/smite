// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/node/node.h>
#include <tendermint/node/plugin.h>
#include <iostream>

using namespace tendermint::node;

namespace tendermint {

struct NodePluginImpl {
  NodePluginImpl(appbase::application& app): app(app) {}

  std::optional<Node> node;

  appbase::application& app;
};

NodePlugin::NodePlugin(appbase::application& app): plugin(app, "tendermint::node"), my(new NodePluginImpl(app)) {}

void NodePlugin::set_program_options(CLI::App&) {}

void NodePlugin::plugin_initialize(const CLI::App& config) {
  my->node.emplace();
}

void NodePlugin::plugin_startup() {
  my->node->start();
}

void NodePlugin::plugin_shutdown() {
  my->node->stop();
}

Node& NodePlugin::node() {
  return *my->node;
}

const Node& NodePlugin::node() const {
  return *my->node;
}

} // namespace tendermint
