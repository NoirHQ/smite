#pragma once
#include <noir/config/base.h>
#include <noir/config/consensus.h>
#include <noir/config/mempool.h>
#include <noir/config/privval.h>
#include <noir/config/tx_index.h>

namespace noir::config {

struct Config : public BaseConfig {
  ConsensusConfig consensus{};
  MempoolConfig mempool{};
  PrivValidatorConfig priv_validator{};
  TxIndexConfig tx_index{};

  void bind(CLI::App& cmd) {
    BaseConfig::bind(cmd);
    mempool.bind(cmd);
    priv_validator.bind(cmd);
  }
};

} // namespace noir::config
