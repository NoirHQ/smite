#include <noir/tendermint/tendermint.h>
#include <appbase/application.hpp>

using namespace noir::tendermint;

namespace noir::commands {

CLI::App* start(CLI::App& root) {
  return root.add_subcommand("start", "Run the NOIR node")->final_callback([]() {
    auto& app = appbase::app();
    auto home_dir = app.home_dir();
    config::set("home", home_dir.c_str());
    config::load();

    if (!app.initialize<class tendermint>()) {
      throw CLI::Success();
    }
    app.startup();
    app.exec();
  });
}

} // namespace noir::commands
