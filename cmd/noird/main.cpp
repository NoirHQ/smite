#include <noir/tendermint/tendermint.h>

#include <appbase/application.hpp>

using appbase::app;

int main(int argc, char** argv) {
  app().register_plugin<noir::tendermint>();
  if (!app().initialize<noir::tendermint>(argc, argv)) {
    const auto& opts = app().get_options();
    if(opts.count("help") || opts.count("version") || opts.count("full-version") || opts.count("print-default-config")) {
      return 0;
    }
    return 1;
  }
  app().startup();
  app().exec();
  return 0;
}
