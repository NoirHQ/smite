// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <appbase/application.hpp>

appbase::application app;

std::string log_level = "info";

void init_app() {
  app.config().require_subcommand();
  app.config().failure_message(CLI::FailureMessage::help);
  app.config().name("smite");
  app.config().description("BFT state machine replication for applications in any programming languages");

  app.config().get_option("--config")->group("");
  app.config().get_option("--plugin")->group("")->configurable(false);

  app.config().add_option("--log-level", log_level, "log level (default \"info\")");
}
