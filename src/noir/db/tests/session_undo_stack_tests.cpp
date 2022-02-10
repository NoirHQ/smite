// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <noir/db/rocks_session.h>
#include <noir/db/session.h>
#include <noir/db/undo_stack.h>
#include "data_store_tests.h"

#include <random>

using namespace noir::db::session;
using namespace noir::db::session_tests;

TEST_CASE("undo_stack_test", "session_undo_stack_tests") {
  // Push the head session into the undo stack.
  auto data_store = noir::db::session::make_session(make_rocks_db(), 16);
  auto undo = noir::db::session::undo_stack(data_store);
  auto session_kvs_1 = std::unordered_map<uint16_t, uint16_t>{
    {1, 100},
    {2, 200},
    {3, 300},
    {4, 400},
    {5, 500},
  };
  write(data_store, session_kvs_1);
  verify_equal(data_store, session_kvs_1, int_t{});

  auto top = [&]() -> decltype(undo)::session_type& {
    return *std::get<decltype(undo)::session_type*>(undo.top().holder());
  };

  // Push a new session on the end of the undo stack and write some data to it
  undo.push();
  CHECK(undo.revision() == 1);

  auto session_kvs_2 = std::unordered_map<uint16_t, uint16_t>{
    {6, 600},
    {7, 700},
    {8, 800},
    {9, 900},
    {10, 1000},
  };
  write(top(), session_kvs_2);
  verify_equal(top(), collapse({session_kvs_1, session_kvs_2}), int_t{});

  // Undo that new session
  undo.undo();
  CHECK(undo.revision() == 0);
  CHECK(undo.empty());
  verify_equal(data_store, session_kvs_1, int_t{});

  // Push a new session and verify.
  undo.push();
  CHECK(undo.revision() == 1);
  write(top(), session_kvs_2);
  verify_equal(top(), collapse({session_kvs_1, session_kvs_2}), int_t{});

  auto session_kvs_3 = std::unordered_map<uint16_t, uint16_t>{
    {11, 1100},
    {12, 1200},
    {13, 1300},
    {14, 1400},
    {15, 1500},
  };
  undo.push();
  CHECK(undo.revision() == 2);
  write(top(), session_kvs_3);
  verify_equal(top(), collapse({session_kvs_1, session_kvs_2, session_kvs_3}), int_t{});

  undo.squash();
  CHECK(undo.revision() == 1);
  verify_equal(top(), collapse({session_kvs_1, session_kvs_2, session_kvs_3}), int_t{});

  auto session_kvs_4 = std::unordered_map<uint16_t, uint16_t>{
    {16, 1600},
    {17, 1700},
    {18, 1800},
    {19, 1900},
    {20, 2000},
  };
  undo.push();
  CHECK(undo.revision() == 2);
  write(top(), session_kvs_4);
  verify_equal(top(), collapse({session_kvs_1, session_kvs_2, session_kvs_3, session_kvs_4}), int_t{});

  auto session_kvs_5 = std::unordered_map<uint16_t, uint16_t>{
    {21, 2100},
    {22, 2200},
    {23, 2300},
    {24, 2400},
    {25, 2500},
  };
  undo.push();
  CHECK(undo.revision() == 3);
  write(top(), session_kvs_5);
  verify_equal(top(), collapse({session_kvs_1, session_kvs_2, session_kvs_3, session_kvs_4, session_kvs_5}), int_t{});

  auto session_kvs_6 = std::unordered_map<uint16_t, uint16_t>{
    {26, 2600},
    {27, 2700},
    {28, 2800},
    {29, 2900},
    {30, 3000},
  };
  undo.push();
  CHECK(undo.revision() == 4);
  write(top(), session_kvs_6);
  verify_equal(top(),
    collapse({session_kvs_1, session_kvs_2, session_kvs_3, session_kvs_4, session_kvs_5, session_kvs_6}), int_t{});

  // Commit revision 3 and verify that the top session has the correct key values.
  undo.commit(3);
  CHECK(undo.revision() == 4);
  verify_equal(top(),
    collapse({session_kvs_1, session_kvs_2, session_kvs_3, session_kvs_4, session_kvs_5, session_kvs_6}), int_t{});
}
