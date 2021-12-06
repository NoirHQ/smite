#pragma once
#include <appbase/application.hpp>

namespace noir {

using appbase::options_description;
using appbase::variables_map;

class app_sm : public appbase::plugin<app_sm> {
public:
  APPBASE_PLUGIN_REQUIRES()

  app_sm();

  virtual void set_program_options(options_description &, options_description &cfg) override {}

  void plugin_initialize(const variables_map &options);
  void plugin_startup();
  void plugin_shutdown();

  void new_txn();
  void new_db_height(int64_t height);
  void commit_txn();
  void apply_db();

  void put_tx(std::string key, std::string value);
  std::string commit();

  int64_t get_last_height();
  std::string& get_last_apphash();

private:
  void init_app_state();
  std::string_view get_app_state(std::string key);
  void put_app_state(std::string key, std::string value);

  int64_t last_height;
  std::string last_apphash;
};

}
