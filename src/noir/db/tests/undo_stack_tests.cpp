// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <catch2/catch_all.hpp>
#include <boost/filesystem.hpp>
#include "chain_kv_tests.h"

using noir::db::chain_kv::bytes;
using noir::db::chain_kv::to_slice;

void undo_tests(bool reload_undo, uint64_t target_segment_size) {
  boost::filesystem::remove_all("test-undo-db");
  noir::db::chain_kv::database db{"test-undo-db", true};
  std::unique_ptr<noir::db::chain_kv::undo_stack> undo_stack;

  auto reload = [&] {
    if (!undo_stack || reload_undo)
      undo_stack = std::make_unique<noir::db::chain_kv::undo_stack>(db, bytes{0x10}, target_segment_size);
  };
  reload();

  KV_REQUIRE_EXCEPTION(undo_stack->undo(), "nothing to undo");
  CHECK(undo_stack->revision() == 0);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x00}, to_slice({}));
    session.set({0x20, 0x02}, to_slice({0x50}));
    session.set({0x20, 0x01}, to_slice({0x40}));
    session.erase({0x20, 0x02});
    session.set({0x20, 0x03}, to_slice({0x60}));
    session.set({0x20, 0x01}, to_slice({0x50}));
    session.write_changes(*undo_stack);
  }
  KV_REQUIRE_EXCEPTION(undo_stack->undo(), "nothing to undo");
  CHECK(undo_stack->revision() == 0);
  CHECK(get_all(db, {0x10, (char)0x80}) == (kv_values{})); // no undo segments
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x00}, {}},
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x03}, {0x60}},
    }}));

  reload();
  undo_stack->push();
  CHECK(undo_stack->revision() == 1);
  reload();
  CHECK(undo_stack->revision() == 1);
  {
    noir::db::chain_kv::write_session session{db};
    session.erase({0x20, 0x01});
    session.set({0x20, 0x00}, to_slice({0x70}));
    session.write_changes(*undo_stack);
  }
  CHECK_FALSE(get_all(db, {0x10, (char)0x80}) == (kv_values{})); // has undo segments
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x00}, {0x70}},
      {{0x20, 0x03}, {0x60}},
    }}));

  CHECK(undo_stack->revision() == 1);
  reload();
  CHECK(undo_stack->revision() == 1);
  KV_REQUIRE_EXCEPTION(undo_stack->set_revision(2), "cannot set revision while there is an existing undo stack");
  undo_stack->undo();
  CHECK(get_all(db, {0x10, (char)0x80}) == (kv_values{})); // no undo segments
  CHECK(undo_stack->revision() == 0);
  reload();
  CHECK(undo_stack->revision() == 0);
  undo_stack->set_revision(10);
  CHECK(undo_stack->revision() == 10);
  reload();
  CHECK(undo_stack->revision() == 10);

  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x00}, {}},
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x03}, {0x60}},
    }}));

  {
    noir::db::chain_kv::write_session session{db};
    session.erase({0x20, 0x01});
    session.set({0x20, 0x00}, to_slice({0x70}));
    session.write_changes(*undo_stack);
  }
  CHECK(get_all(db, {0x10, (char)0x80}) == (kv_values{})); // no undo segments
  reload();
  undo_stack->push();
  CHECK(undo_stack->revision() == 11);
  reload();
  CHECK(undo_stack->revision() == 11);
  KV_REQUIRE_EXCEPTION(undo_stack->set_revision(12), "cannot set revision while there is an existing undo stack");
  undo_stack->commit(0);
  CHECK(undo_stack->revision() == 11);
  KV_REQUIRE_EXCEPTION(undo_stack->set_revision(12), "cannot set revision while there is an existing undo stack");
  undo_stack->commit(11);
  CHECK(undo_stack->revision() == 11);
  reload();
  KV_REQUIRE_EXCEPTION(undo_stack->set_revision(9), "revision cannot decrease");
  undo_stack->set_revision(12);
  CHECK(undo_stack->revision() == 12);
  reload();
  CHECK(undo_stack->revision() == 12);

  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x00}, {0x70}},
      {{0x20, 0x03}, {0x60}},
    }}));
} // undo_tests()

void squash_tests(bool reload_undo, uint64_t target_segment_size) {
  boost::filesystem::remove_all("test-squash-db");
  noir::db::chain_kv::database db{"test-squash-db", true};
  std::unique_ptr<noir::db::chain_kv::undo_stack> undo_stack;

  auto reload = [&] {
    if (!undo_stack || reload_undo)
      undo_stack = std::make_unique<noir::db::chain_kv::undo_stack>(db, bytes{0x10}, target_segment_size);
  };
  reload();

  // set 1
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 1);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x01}, to_slice({0x50}));
    session.set({0x20, 0x02}, to_slice({0x60}));
    session.write_changes(*undo_stack);
  }
  reload();
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x60}},
    }}));

  // set 2
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 2);
  {
    noir::db::chain_kv::write_session session{db};
    session.erase({0x20, 0x01});
    session.set({0x20, 0x02}, to_slice({0x61}));
    session.set({0x20, 0x03}, to_slice({0x70}));
    session.set({0x20, 0x04}, to_slice({0x10}));
    session.write_changes(*undo_stack);
  }
  reload();
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 3);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x01}, to_slice({0x50}));
    session.set({0x20, 0x02}, to_slice({0x62}));
    session.erase({0x20, 0x03});
    session.set({0x20, 0x05}, to_slice({0x05}));
    session.set({0x20, 0x06}, to_slice({0x06}));
    session.write_changes(*undo_stack);
  }
  reload();
  undo_stack->squash();
  reload();
  CHECK(undo_stack->revision() == 2);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
    }}));

  // set 3
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 3);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x07}, to_slice({0x07}));
    session.set({0x20, 0x08}, to_slice({0x08}));
    session.write_changes(*undo_stack);
  }
  reload();
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 4);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x09}, to_slice({0x09}));
    session.set({0x20, 0x0a}, to_slice({0x0a}));
    session.write_changes(*undo_stack);
  }
  reload();
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 5);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x0b}, to_slice({0x0b}));
    session.set({0x20, 0x0c}, to_slice({0x0c}));
    session.write_changes(*undo_stack);
  }
  reload();
  undo_stack->squash();
  reload();
  CHECK(undo_stack->revision() == 4);
  undo_stack->squash();
  reload();
  CHECK(undo_stack->revision() == 3);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
      {{0x20, 0x07}, {0x07}},
      {{0x20, 0x08}, {0x08}},
      {{0x20, 0x09}, {0x09}},
      {{0x20, 0x0a}, {0x0a}},
      {{0x20, 0x0b}, {0x0b}},
      {{0x20, 0x0c}, {0x0c}},
    }}));

  // undo set 3
  undo_stack->undo();
  reload();
  CHECK(undo_stack->revision() == 2);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
    }}));

  // undo set 2
  undo_stack->undo();
  reload();
  CHECK(undo_stack->revision() == 1);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x60}},
    }}));

  // undo set 1
  undo_stack->undo();
  reload();
  CHECK(undo_stack->revision() == 0);
  CHECK(get_all(db, {0x20}) == (kv_values{{}}));

  // TODO: test squash with only 1 undo level
} // squash_tests()

void commit_tests(bool reload_undo, uint64_t target_segment_size) {
  boost::filesystem::remove_all("test-commit-db");
  noir::db::chain_kv::database db{"test-commit-db", true};
  std::unique_ptr<noir::db::chain_kv::undo_stack> undo_stack;

  auto reload = [&] {
    if (!undo_stack || reload_undo)
      undo_stack = std::make_unique<noir::db::chain_kv::undo_stack>(db, bytes{0x10}, target_segment_size);
  };
  reload();

  // revision 1
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 1);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x01}, to_slice({0x50}));
    session.set({0x20, 0x02}, to_slice({0x60}));
    session.write_changes(*undo_stack);
  }
  reload();
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x60}},
    }}));

  // revision 2
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 2);
  {
    noir::db::chain_kv::write_session session{db};
    session.erase({0x20, 0x01});
    session.set({0x20, 0x02}, to_slice({0x61}));
    session.set({0x20, 0x03}, to_slice({0x70}));
    session.set({0x20, 0x04}, to_slice({0x10}));
    session.set({0x20, 0x01}, to_slice({0x50}));
    session.set({0x20, 0x02}, to_slice({0x62}));
    session.erase({0x20, 0x03});
    session.set({0x20, 0x05}, to_slice({0x05}));
    session.set({0x20, 0x06}, to_slice({0x06}));
    session.write_changes(*undo_stack);
  }
  reload();
  CHECK(undo_stack->revision() == 2);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
    }}));

  // revision 3
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 3);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x07}, to_slice({0x07}));
    session.set({0x20, 0x08}, to_slice({0x08}));
    session.set({0x20, 0x09}, to_slice({0x09}));
    session.set({0x20, 0x0a}, to_slice({0x0a}));
    session.set({0x20, 0x0b}, to_slice({0x0b}));
    session.set({0x20, 0x0c}, to_slice({0x0c}));
    session.write_changes(*undo_stack);
  }
  reload();
  CHECK(undo_stack->revision() == 3);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
      {{0x20, 0x07}, {0x07}},
      {{0x20, 0x08}, {0x08}},
      {{0x20, 0x09}, {0x09}},
      {{0x20, 0x0a}, {0x0a}},
      {{0x20, 0x0b}, {0x0b}},
      {{0x20, 0x0c}, {0x0c}},
    }}));

  // commit revision 1
  undo_stack->commit(1);

  // undo revision 3
  undo_stack->undo();
  reload();
  CHECK(undo_stack->revision() == 2);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
    }}));

  // undo revision 2
  undo_stack->undo();
  reload();
  CHECK(undo_stack->revision() == 1);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x60}},
    }}));

  // Can't undo revision 1
  KV_REQUIRE_EXCEPTION(undo_stack->undo(), "nothing to undo");

  CHECK(get_all(db, {0x10, (char)0x80}) == (kv_values{})); // no undo segments

  // revision 2
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 2);
  {
    noir::db::chain_kv::write_session session{db};
    session.erase({0x20, 0x01});
    session.set({0x20, 0x02}, to_slice({0x61}));
    session.set({0x20, 0x03}, to_slice({0x70}));
    session.set({0x20, 0x04}, to_slice({0x10}));
    session.set({0x20, 0x01}, to_slice({0x50}));
    session.set({0x20, 0x02}, to_slice({0x62}));
    session.erase({0x20, 0x03});
    session.set({0x20, 0x05}, to_slice({0x05}));
    session.set({0x20, 0x06}, to_slice({0x06}));
    session.write_changes(*undo_stack);
  }
  reload();
  CHECK(undo_stack->revision() == 2);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
    }}));

  // revision 3
  undo_stack->push();
  reload();
  CHECK(undo_stack->revision() == 3);
  {
    noir::db::chain_kv::write_session session{db};
    session.set({0x20, 0x07}, to_slice({0x07}));
    session.set({0x20, 0x08}, to_slice({0x08}));
    session.set({0x20, 0x09}, to_slice({0x09}));
    session.set({0x20, 0x0a}, to_slice({0x0a}));
    session.set({0x20, 0x0b}, to_slice({0x0b}));
    session.set({0x20, 0x0c}, to_slice({0x0c}));
    session.write_changes(*undo_stack);
  }
  reload();

  // commit revision 3
  undo_stack->commit(3);

  // Can't undo
  KV_REQUIRE_EXCEPTION(undo_stack->undo(), "nothing to undo");

  CHECK(get_all(db, {0x10, (char)0x80}) == (kv_values{})); // no undo segments

  reload();
  CHECK(undo_stack->revision() == 3);
  CHECK(get_all(db, {0x20}) ==
    (kv_values{{
      {{0x20, 0x01}, {0x50}},
      {{0x20, 0x02}, {0x62}},
      {{0x20, 0x04}, {0x10}},
      {{0x20, 0x05}, {0x05}},
      {{0x20, 0x06}, {0x06}},
      {{0x20, 0x07}, {0x07}},
      {{0x20, 0x08}, {0x08}},
      {{0x20, 0x09}, {0x09}},
      {{0x20, 0x0a}, {0x0a}},
      {{0x20, 0x0b}, {0x0b}},
      {{0x20, 0x0c}, {0x0c}},
    }}));
} // commit_tests()

TEST_CASE("test_undo", "undo_stack_tests") {
  undo_tests(false, 0);
  undo_tests(true, 0);
  undo_tests(false, 64 * 1024 * 1024);
  undo_tests(true, 64 * 1024 * 1024);
}

TEST_CASE("test_squash", "undo_stack_tests") {
  squash_tests(false, 0);
  squash_tests(true, 0);
  squash_tests(false, 64 * 1024 * 1024);
  squash_tests(true, 64 * 1024 * 1024);
}

TEST_CASE("test_commit", "undo_stack_tests") {
  commit_tests(false, 0);
  commit_tests(true, 0);
  commit_tests(false, 64 * 1024 * 1024);
  commit_tests(true, 64 * 1024 * 1024);
}
