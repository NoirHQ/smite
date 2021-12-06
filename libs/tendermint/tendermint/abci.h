#ifndef ABCI_H
#define ABCI_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ResponseInfo_ {
  int64_t last_block_height;
  char *last_block_app_hash;
} ResponseInfo;

void begin_block(int64_t height);
void deliver_tx(void* tx, unsigned int len);
void end_block();
char* commit();
void check_tx(void* tx, unsigned int len);
ResponseInfo* info();

#ifdef __cplusplus
}
#endif

#endif /* ABCI_H */
