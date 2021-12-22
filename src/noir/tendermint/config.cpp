#include <tendermint/tendermint.h>

namespace noir::tendermint::config {

bool load() {
  return tm_config_load();
}

bool save() {
  return tm_config_save();
}

void set(const char* key, const char* value) {
  tm_config_set(key, value);
}

} // namespace noir::tendermint::config
