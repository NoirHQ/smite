#include <noir/app/app.h>

#include <tendermint/abci.h>
#include <tendermint/tendermint.h>

noir::app* tm_app;

void begin_block(int64_t height) {
  tm_ilog("!!! BeginBlock !!!");
  tm_app->begin_block(height);
}

void deliver_tx(void* tx, unsigned int len) {
  tm_ilog("!!! DeliverTx !!!");
  std::string bytes(static_cast<char*>(tx), len);
  tm_app->deliver_tx(bytes);
}

void end_block() {
  tm_ilog("!!! EndBlock !!!");
  tm_app->end_block();
}

char* commit() {
  tm_ilog("!!! Commit !!!");
  std::string apphash = tm_app->commit();
  char* cString = strdup(apphash.c_str()); // MUST be freed by golang
  return cString;
}

void check_tx(void* tx, unsigned int len) {
  std::string bytes(static_cast<char*>(tx), len);
  tm_ilog("!!! --- CheckTX --- !!!");
}

ResponseInfo* info() {
  auto* info = (ResponseInfo*) malloc(sizeof(ResponseInfo));
  info->last_block_height = tm_app->get_last_height();
  info->last_block_app_hash = tm_app->get_last_apphash().data(); // MUST NOT be freed by golang
  return info;
}
