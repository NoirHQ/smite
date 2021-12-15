#include <noir/tendermint/config.h>
#include <tendermint/tendermint.h>

namespace noir::tendermint::config {

bool load(const char* config_file) {
  return tm_config_load(config_file);
}

bool save() {
  return tm_config_save();
}

void set(const char* key, const char* value) {
  tm_config_set(key, value);
}

} // namespace noir::tendermint::config
