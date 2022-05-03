#pragma once
#include <noir/common/bytes.h>
#include <noir/core/channel.h>
#include <noir/core/result.h>
#include <tendermint/p2p/types.h>
#include <tendermint/types/node_id.h>
#include <cstdint>
#include <memory>

namespace tendermint::p2p {

using namespace noir;

struct Envelope {};

using EnvelopePtr = std::shared_ptr<Envelope>;

struct PeerError {
  NodeId node_id;
  Error err;
};

using PeerErrorPtr = std::shared_ptr<PeerError>;

struct ChannelIterator {
  Chan<EnvelopePtr> pipe;
  EnvelopePtr current;
};

class Channel {
public:
  auto send(EnvelopePtr envelope) -> Result<void>;

  auto send_error(PeerErrorPtr peer_error) -> Result<void>;

  auto to_string() -> std::string;

  auto receive() -> ChannelIterator;

public:
  Channel(ChannelId id, Chan<EnvelopePtr>&& in_ch, Chan<EnvelopePtr>&& out_ch, Chan<EnvelopePtr>&& err_ch)
    : id(id), in_ch(std::move(in_ch)), out_ch(std::move(out_ch)), err_ch(std::move(err_ch)) {}

  ChannelId id;
  Chan<EnvelopePtr> in_ch;
  Chan<EnvelopePtr> out_ch;
  Chan<EnvelopePtr> err_ch;

  // messageType
  std::string name;
};

} // namespace tendermint::p2p
