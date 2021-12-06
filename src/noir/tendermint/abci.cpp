#include <tendermint/abci.h>
#include <tendermint/tendermint.h>

void begin_block(int64_t height) {
  tm_ilog("!!! BeginBlock !!!");
}

void deliver_tx(void* tx, unsigned int len) {
  tm_ilog("!!! DeliverTx !!!");
}

void end_block() {
  tm_ilog("!!! EndBlock !!!");
}

char* commit() {
  tm_ilog("!!! Commit !!!");
  return nullptr;
}

void check_tx(void* tx, unsigned int len) {
  tm_ilog("!!! --- CheckTX --- !!!");
}

ResponseInfo *info() {
  auto *info = (ResponseInfo *) calloc(1, sizeof(ResponseInfo));
  return info;
}
