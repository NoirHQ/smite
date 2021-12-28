#include <tendermint/tendermint.h>

namespace noir::tendermint::log {

void info(const char* msg) {
  tm_log_info(msg);
}

void debug(const char* msg) {
  tm_log_debug(msg);
}

void error(const char* msg) {
  tm_log_error(msg);
}

} // namespace noir::tendermint::log
