#pragma once

namespace noir::tendermint::config {

bool load(const char* config_file);
bool save();
void set(const char* key, const char* value);

} // namespace noir::tendermint::config

