// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/core/codec.h>
#include <noir/p2p/protocol.h>

namespace noir::p2p {

using send_buffer_type = std::shared_ptr<std::vector<char>>;

struct buffer_factory {

  const send_buffer_type& get_send_buffer(const net_message& m) {
    if (!send_buffer) {
      send_buffer = create_send_buffer(m);
    }
    return send_buffer;
  }

protected:
  send_buffer_type send_buffer;

protected:
  static send_buffer_type create_send_buffer(const net_message& m) {
    const uint32_t payload_size = encode_size(m);
    const char* const header = reinterpret_cast<const char* const>(&payload_size);
    constexpr size_t header_size = sizeof(payload_size);
    static_assert(header_size == message_header_size, "invalid message_header_size");
    const size_t buffer_size = header_size + payload_size;

    auto send_buffer = std::make_shared<std::vector<char>>(buffer_size);
    datastream<unsigned char> ds(reinterpret_cast<unsigned char*>(send_buffer->data()), buffer_size);
    ds.write(header, header_size);
    ds << m;

    return send_buffer;
  }
};

} // namespace noir::p2p
