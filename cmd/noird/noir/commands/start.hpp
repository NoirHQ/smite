#pragma once
#include <appbase/application.hpp>

namespace noir::commands {

CLI::App* start(CLI::App& root) {
  return root.add_subcommand("start", "Run the NOIR node")->final_callback([]() {
    using noir::tendermint::tendermint;

    auto& app = appbase::app();
    auto home_dir = app.home_dir();
    noir::tendermint::config::set("home", home_dir.c_str());
    noir::tendermint::config::load();

    if (!app.initialize<tendermint>()) {
      throw CLI::Success();
    }
    app.startup();
    app.exec();
  });
}

} // namespace noir::commands
