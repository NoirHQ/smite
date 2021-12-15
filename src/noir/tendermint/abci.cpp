#include <noir/tendermint/log.h>
#include <tendermint/abci.h>

namespace tmlog = noir::tendermint::log;

void begin_block(int64_t height) {
  tmlog::info("!!! BeginBlock !!!");
}

void deliver_tx(void* tx, unsigned int len) {
  tmlog::info("!!! DeliverTx !!!");
}

void end_block() {
  tmlog::info("!!! EndBlock !!!");
}

char* commit() {
  tmlog::info("!!! Commit !!!");
  return nullptr;
}

void check_tx(void* tx, unsigned int len) {
  tmlog::info("!!! --- CheckTX --- !!!");
}

ResponseInfo *info() {
  auto *info = (ResponseInfo *) calloc(1, sizeof(ResponseInfo));
  return info;
}
