#include <noir/net/types.h>
#include <fc/crypto/sha256.hpp>

namespace noir::net {

using namespace fc;


enum signed_msg_type {
  Unknown = 0,
  Prevote = 1,
  Precommit = 2,
  Proposal = 32
};

struct part_set_header {
  uint32_t total;
  sha256 hash;
};

struct block_id {
  sha256 hash;
  part_set_header parts;
};

struct vote_extension {
  bytes app_data_to_sign;
  bytes app_data_self_authenticating;
};

struct proposal_message {
  signed_msg_type type;
  int64_t height;
  int32_t round;
  int32_t pol_round;
  block_id block_id;
  tstamp timestamp{0};
  signature sig;
};

struct block_part_message {
  int64_t height;
  int32_t round;
  uint32_t index;
  bytes bs;
  bytes proof;
};

struct vote_message {
  signed_msg_type type;
  int64_t height;
  int32_t round;
  block_id block_id;
  tstamp timestamp;
  bytes validator_address;
  int32_t validator_index;
  signature sig;
  vote_extension vote_extension;
};

using net_message = std::variant<proposal_message,
                                 block_part_message,
                                 vote_message>;

} // namespace noir::net
