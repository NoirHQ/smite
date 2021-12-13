#pragma once
#include <appbase/application.hpp>

namespace noir {

class tendermint : public appbase::plugin<tendermint> {
public:
  APPBASE_PLUGIN_REQUIRES()

  virtual void set_program_options(CLI::App& cli, CLI::App& config) override {}

  void plugin_initialize(const CLI::App& cli, const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();
};

}