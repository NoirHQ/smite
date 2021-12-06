#include <noir/app/app.h>

#include <fc/log/logger.hpp>
#include <tendermint/tendermint.h>

namespace noir {

void app::plugin_initialize(const variables_map &options) {
  ilog("app init");
}

void app::plugin_startup() {
  ilog("app start");
}

void app::plugin_shutdown() {
  ilog("app stop");
}

int app::deliver_tx(std::string& tx) {
  ilog("processing deliver_tx: ${tx}", ("tx", tx));
  appbase::app().find_plugin<app_sm>()->put_tx(tx, tx);
  return 0;
}

std::string app::commit() {
  return appbase::app().find_plugin<app_sm>()->commit();
}

void app::begin_block(int64_t height) {
  appbase::app().find_plugin<app_sm>()->new_txn();
  appbase::app().find_plugin<app_sm>()->new_db_height(height);
}

void app::end_block() {
}

int64_t app::get_last_height() {
  return appbase::app().find_plugin<app_sm>()->get_last_height();
}

std::string &app::get_last_apphash() {
  return appbase::app().find_plugin<app_sm>()->get_last_apphash();
}

}
