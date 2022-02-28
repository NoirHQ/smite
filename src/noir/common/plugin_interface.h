// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/protocol.h>
#include <noir/consensus/types/round_state.h>
#include <noir/p2p/protocol.h>

#include <appbase/application.hpp>
#include <appbase/channel.hpp>

namespace noir::plugin_interface {

using event_message = std::variant<consensus::round_state, p2p::vote_message>;
struct event_info {
  consensus::event_type event_;
  event_message message_;
};
using event_info_ptr = std::shared_ptr<event_info>;

namespace channels {
  using timeout_ticker = appbase::channel_decl<struct timeout_ticker_tag, consensus::timeout_info_ptr>;
  using internal_message_queue = appbase::channel_decl<struct internal_message_queue_tag, p2p::internal_msg_info_ptr>;
} // namespace channels

namespace methods {}

namespace incoming {
  namespace channels {
    using receive_message_queue = appbase::channel_decl<struct receive_message_queue_tag, p2p::envelope_ptr>;
  }
} // namespace incoming

namespace egress {
  namespace channels {
    using transmit_message_queue = appbase::channel_decl<struct transmit_message_queue_tag, p2p::envelope_ptr>;
    using event_switch_message_queue = appbase::channel_decl<struct event_switch_message_queue_tag, event_info_ptr>;
  } // namespace channels
} // namespace egress

} // namespace noir::plugin_interface
