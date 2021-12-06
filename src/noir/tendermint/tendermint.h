#pragma once
#include <appbase/application.hpp>

namespace noir {

using appbase::options_description;
using appbase::variables_map;

class tendermint : public appbase::plugin<tendermint> {
public:
  APPBASE_PLUGIN_REQUIRES()

  virtual void set_program_options(options_description &, options_description &cfg) override {}

  void plugin_initialize(const variables_map &options);
  void plugin_startup();
  void plugin_shutdown();
};

}