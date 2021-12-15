#pragma once

namespace noir::tendermint::log {

void info(const char* msg);
void debug(const char* msg);
void error(const char* msg);

} // namespace noir::tendermint::log
