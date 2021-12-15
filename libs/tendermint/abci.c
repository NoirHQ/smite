#include "tendermint/abci.h"

#define WEAK __attribute__((weak))

WEAK void begin_block(int64_t height) {
}

WEAK void deliver_tx(void* tx, unsigned int len) {
}

WEAK void end_block() {
}

WEAK char* commit() {
  return NULL;
}

WEAK void check_tx(void* tx, unsigned int len) {
}

WEAK ResponseInfo* info() {
  return NULL;
}

