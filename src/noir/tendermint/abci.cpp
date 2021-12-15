#include <noir/tendermint/tendermint.h>
#include <tendermint/abci.h>

namespace log = noir::tendermint::log;

void begin_block(int64_t height) {
  log::info("!!! BeginBlock !!!");
}

void deliver_tx(void* tx, unsigned int len) {
  log::info("!!! DeliverTx !!!");
}

void end_block() {
  log::info("!!! EndBlock !!!");
}

char* commit() {
  log::info("!!! Commit !!!");
  return nullptr;
}

void check_tx(void* tx, unsigned int len) {
  log::info("!!! --- CheckTX --- !!!");
}

ResponseInfo *info() {
  auto *info = (ResponseInfo *) calloc(1, sizeof(ResponseInfo));
  return info;
}
