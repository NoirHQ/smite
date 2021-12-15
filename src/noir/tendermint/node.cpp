#include <noir/tendermint/node.h>
#include <tendermint/tendermint.h>

namespace noir::tendermint::node {

void start() {
  tm_node_start();
}

void stop() {
  tm_node_stop();
}

} // namespace noir::tendermint::node
