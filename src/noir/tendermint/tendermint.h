#pragma once
#include <noir/tendermint/config.h>
#include <noir/tendermint/log.h>
#include <noir/tendermint/node.h>
#include <appbase/application.hpp>

namespace noir::tendermint {

class tendermint : public appbase::plugin<tendermint> {
public:
  APPBASE_PLUGIN_REQUIRES()

  virtual void set_program_options(CLI::App& cli, CLI::App& config) override {}

  void plugin_initialize(const CLI::App& cli, const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();
};

} // namespace noir::tendermint
