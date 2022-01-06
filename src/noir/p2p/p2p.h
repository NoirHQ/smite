// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/p2p/protocol.h>
#include <appbase/application.hpp>

namespace noir::p2p {

struct connection_status {
  std::string peer;
  bool connecting = false;
  bool syncing = false;
  handshake_message last_handshake;
};

class p2p : public appbase::plugin<p2p> {
public:
  APPBASE_PLUGIN_REQUIRES()

  p2p();
  virtual ~p2p();

  void set_program_options(CLI::App& cli, CLI::App& config) override;

  void plugin_initialize(const CLI::App& cli, const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

  std::string connect(const std::string& host);
  std::string disconnect(const std::string& host);
  std::optional<connection_status> status(const std::string& endpoint) const;
  std::vector<connection_status> connections() const;

private:
  std::shared_ptr<class p2p_impl> my;
};

} // namespace noir::p2p

