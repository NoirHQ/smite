// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <tendermint/p2p/conn/connection.h>
#include <iostream>

using namespace tendermint::p2p::conn;

TEST_CASE("Connection", "[tendermint][p2p][conn]") {
  SECTION("Calc Max Packet Size") {
    auto max_packet_msg_size = MConnection::calc_max_packet_msg_size(4096);
    std::cout << max_packet_msg_size << std::endl;
  }
}