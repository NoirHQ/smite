#include <noir/net/net_plugin.h>
#include <appbase/application.hpp>

using namespace noir::net;

using net_plugin = noir::net::net_plugin;

int main(int argc, char **argv) {
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

  auto &app = appbase::app();

  // set default configuration
  app.cli().failure_message(CLI::FailureMessage::help);
  app.cli().name("net_test");
  app.cli().description("Sample to test net_plugin");
  std::filesystem::path home_dir;
  if (auto arg = std::getenv("HOME")) {
    home_dir = arg;
  }
  app.set_home_dir(home_dir / ".noir");
  app.set_config_file("app.toml");

  // option 1: manually start net_plugin
  app.register_plugin<net_plugin>();

  // prepare command line arguments
//  app.cli().allow_extras(); // is this always required?
//  app.cli().parse(argc, argv);
  app.config().allow_extras(); // is this always required?
  app.config().parse(argc, argv);

  app.initialize<net_plugin>();
  app.startup();
  app.exec();

  // option 2: auto start plugins
//  app.register_plugin<net_plugin>();
//  app.run(argc, argv);
  return 0;
}
