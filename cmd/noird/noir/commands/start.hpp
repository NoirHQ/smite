#pragma once
#include <appbase/application.hpp>

namespace noir::commands {

CLI::App* start(CLI::App& root) {
  return root.add_subcommand("start", "Run the NOIR node")->final_callback([]() {
    using noir::tendermint::tendermint;

    auto& app = appbase::app();
    auto config_file = app.config_file();
    noir::tendermint::config::load(config_file.c_str());

    if (!app.initialize<tendermint>()) {
      throw CLI::Success();
    }
    app.startup();
    app.exec();
  });
}

} // namespace noir::commands
