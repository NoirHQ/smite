#pragma once
#include <noir/net/types.h>

namespace noir::net::consensus { // todo - move consensus somewhere (maybe under libs?)

struct validator {
  bytes address;
  bytes pub_key;
  int64_t voting_power;
  int64_t proposer_priority;
};

struct validator_set {
  std::vector<validator> validators;
  validator proposer;
  int64_t total_voting_power;
};

}
