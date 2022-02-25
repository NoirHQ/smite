// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/protocol.h>
#include <noir/p2p/protocol.h>

#include <appbase/application.hpp>
#include <appbase/channel.hpp>

namespace noir::plugin_interface {

namespace channels {
  using timeout_ticker = appbase::channel_decl<struct timeout_ticker_tag, consensus::timeout_info_ptr>;
  using internal_message_queue = appbase::channel_decl<struct internal_message_queue_tag, p2p::msg_info_ptr>;
} // namespace channels

namespace methods {}

namespace incoming {
  namespace channels {
    using peer_message_queue = appbase::channel_decl<struct peer_message_queue_tag, p2p::msg_info_ptr>;
  }
} // namespace incoming

namespace egress {
  namespace channels {
    using broadcast_message_queue = appbase::channel_decl<struct broadcast_message_queue_tag, std::span<const char>>;
  }
} // namespace incoming

} // namespace noir::plugin_interface
