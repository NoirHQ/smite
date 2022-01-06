// SPDX-License-Identifier: MIT
// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
//
#pragma once
#include <noir/net/protocol.h>
#include <appbase/application.hpp>

namespace noir::net {

struct connection_status {
  std::string peer;
  bool connecting = false;
  bool syncing = false;
  handshake_message last_handshake;
};

class net_plugin : public appbase::plugin<net_plugin> {
public:
  APPBASE_PLUGIN_REQUIRES()

  net_plugin();
  virtual ~net_plugin();

  virtual void set_program_options(CLI::App& cli, CLI::App& config) override;

  void plugin_initialize(const CLI::App& cli, const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

  string connect(const string& host);
  string disconnect(const string& host);
  std::optional<connection_status> status(const string& endpoint) const;
  std::vector<connection_status> connections() const;

private:
  std::shared_ptr<class net_plugin_impl> my;
};

} // namespace noir::net
