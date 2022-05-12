// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <tendermint/p2p/conn.pb.h>
#include <tendermint/p2p/conn/connection.h>
#include <iostream>

using namespace tendermint::p2p::conn;
using namespace tendermint::p2p;
using namespace noir;

TEST_CASE("Connection", "[tendermint][p2p][conn]") {
  SECTION("Calc Max Packet Size") {
    auto max_packet_msg_size = MConnection::calc_max_packet_msg_size(4096);
    CHECK(max_packet_msg_size == 4103);
  }

  SECTION("PacketPing") {
    Packet packet{};
    packet.mutable_packet_ping();
    CHECK(packet.has_packet_ping());

    std::string serialized = packet.SerializeAsString();
    Packet recovered_packet;
    recovered_packet.ParseFromString(serialized);
    CHECK(recovered_packet.has_packet_ping());
  }

  SECTION("PacketPong") {
    Packet packet{};
    packet.mutable_packet_pong();
    CHECK(packet.has_packet_pong());

    std::string serialized = packet.SerializeAsString();
    Packet recovered_packet;
    recovered_packet.ParseFromString(serialized);
    CHECK(recovered_packet.has_packet_pong());
  }

  SECTION("PacketMsg") {
    Packet packet{};
    PacketMsg* msg = packet.mutable_packet_msg();
    msg->set_channel_id(1);
    msg->set_eof(true);
    Bytes data = {'\x00', '\x01', '\x02', '\x03', '\x04'};
    msg->set_data({data.begin(), data.end()});
    CHECK(packet.has_packet_msg());

    std::string serialized = packet.SerializeAsString();
    Packet parsed_packet;
    parsed_packet.ParseFromString(serialized);
    CHECK(parsed_packet.has_packet_msg());
    CHECK(parsed_packet.packet_msg().data()[0] == '\x00');
    CHECK(parsed_packet.packet_msg().data()[4] == '\x04');
  }
}