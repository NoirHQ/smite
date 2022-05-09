// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/types.h>
#include <noir/p2p/p2p.h>

#include <noir/codec/scale.h>

using namespace noir;
using namespace noir::p2p;

TEST_CASE("serialization: net_message - scale", "[noir][p2p]") {
  internal_message m{proposal_message{Proposal, 1, 2, 3, block_id{}, get_time(), {}}};

  // Encode
  const uint32_t payload_size = codec::scale::encode_size(m);
  const char* const header = reinterpret_cast<const char* const>(&payload_size);
  constexpr size_t header_size = sizeof(payload_size);
  static_assert(header_size == message_header_size, "invalid message_header_size");
  const size_t buffer_size = header_size + payload_size;

  auto send_buffer = std::make_shared<std::vector<char>>(buffer_size);
  codec::scale::datastream<char> ds(send_buffer->data(), buffer_size);
  ds.write(header, header_size);
  ds << m;

  std::cout << "header_size=" << header_size << std::endl;
  std::cout << "payload_size=" << payload_size << std::endl;
  std::cout << "ds_size=" << send_buffer->size() << std::endl;
  std::cout << "ds=" << to_hex(*send_buffer) << std::endl;

  // Decode
  codec::scale::datastream<char> ds_decode(send_buffer->data(), buffer_size);
  uint32_t payload_size_decode;
  ds_decode >> payload_size_decode;
  internal_message m_decode;
  ds_decode >> m_decode;
  std::cout << "payload_size_decode=" << payload_size_decode << std::endl;
  std::cout << "m_decode (index)=" << m_decode.index() << std::endl;
  CHECK(std::get<proposal_message>(m).timestamp == std::get<proposal_message>(m_decode).timestamp);
}
