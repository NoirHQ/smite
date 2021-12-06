#include <noir/tendermint/tendermint.h>

#include <tendermint/tendermint.h>

extern noir::app* tm_app;

namespace noir {

void tendermint::plugin_initialize(const variables_map &options) {
  tm_ilog("tendermint init");
  tm_app = appbase::app().find_plugin<app>();
}

void tendermint::plugin_startup() {
  tm_ilog("tendermint start");
  tm_node_start();
}

void tendermint::plugin_shutdown() {
  tm_ilog("tendermint stop");
  tm_node_stop();
}

}
