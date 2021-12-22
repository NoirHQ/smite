#pragma once
#include <appbase/application.hpp>

namespace noir::net {

class net_plugin : public appbase::plugin<net_plugin> {
 public:
  APPBASE_PLUGIN_REQUIRES()

  virtual void set_program_options(CLI::App& cli, CLI::App& config) override {}

  void plugin_initialize(const CLI::App& cli, const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();
};

} // namespace noir::net
