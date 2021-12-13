#include <noir/tendermint/tendermint.h>

#include <appbase/application.hpp>

using appbase::app;

int main(int argc, char** argv) {
  app().cli().name("noird");
  app().cli().description("NOIR Protocol App");
  std::filesystem::path home_dir;
  if (auto arg = std::getenv("HOME")) {
    home_dir = arg;
  }
  app().set_home_dir(home_dir / ".noir");
  app().set_config_file("app.toml");
  app().register_plugin<noir::tendermint>();
  if (!app().initialize<noir::tendermint>(argc, argv)) {
    const auto& opts = app().cli();
    if (opts.count("--help") || opts.count("--print-default-config")) {
      return 0;
    }
    return 1;
  }
  app().startup();
  app().exec();
  return 0;
}
