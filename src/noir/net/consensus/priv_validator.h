#pragma once
#include <noir/net/types.h>

namespace noir::net::consensus {

struct priv_validator {
  bytes pub_key;

  bytes get_pub_key() {
    return pub_key;
  }

  void sign_vote();
  void sign_proposal();

};

}
