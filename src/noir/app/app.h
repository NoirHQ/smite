#pragma once
#include <noir/app_sm/app_sm.h>

#include <appbase/application.hpp>

namespace noir {

using appbase::options_description;
using appbase::variables_map;

class app : public appbase::plugin<app> {
public:
  APPBASE_PLUGIN_REQUIRES((app_sm))

  virtual void set_program_options(options_description &, options_description &cfg) override {}

  void plugin_initialize(const variables_map &options);
  void plugin_startup();
  void plugin_shutdown();

  int deliver_tx(std::string& tx);
  std::string commit();
  void begin_block(int64_t height);
  void end_block();

  int64_t get_last_height();
  std::string& get_last_apphash();
};

}
