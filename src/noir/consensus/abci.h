// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/log.h>
#include <noir/consensus/common_test.h>
#include <noir/consensus/consensus_state.h>

#include <appbase/application.hpp>

namespace noir::consensus {

class abci : public appbase::plugin<abci> {
public:
  APPBASE_PLUGIN_REQUIRES()

  abci() {}
  virtual ~abci() {}

  void set_program_options(CLI::App& config) {}

  void plugin_initialize(const CLI::App& config) {
    ilog("Initialize abci");
  }
  void plugin_startup() {
    auto local_config = config_setup();
    auto [cs1, vss] = rand_cs(local_config, 1);
    auto height = cs1->rs.height;
    auto round = cs1->rs.round;
    start_test_round(cs1, height, round);

    my_cs = move(cs1);
  }
  void plugin_shutdown() {}

  std::unique_ptr<consensus_state> my_cs;
};

} // namespace noir::consensus
