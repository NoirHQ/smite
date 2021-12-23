#include <noir/net/net_plugin.h>
#include <appbase/application.hpp>

using namespace noir::net;

int main(int argc, char **argv) {
  auto& app = appbase::app();

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

  // register plugins
  app.register_plugin<net_plugin>();
  app.initialize<net_plugin>();
  app.startup();
  app.exec();

  //app.run(argc, argv)
  return 0;
}
