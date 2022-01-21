// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <catch2/catch_all.hpp>
#include <boost/filesystem.hpp>
#include "chain_kv_tests.hpp"

using noir::db::chain_kv::bytes;
using noir::db::chain_kv::to_slice;

void view_test(bool reload_session) {
  boost::filesystem::remove_all("view-test-session-db");
  noir::db::chain_kv::database db{"view-test-session-db", true};
  noir::db::chain_kv::undo_stack undo_stack{db, {0x10}};
  std::unique_ptr<noir::db::chain_kv::write_session> session;
  std::unique_ptr<noir::db::chain_kv::view> view;

  auto reload = [&] {
    if (session && reload_session) {
      session->write_changes(undo_stack);
      view = nullptr;
      session = nullptr;
    }
    if (!session)
      session = std::make_unique<noir::db::chain_kv::write_session>(db);
    if (!view)
      view = std::make_unique<noir::db::chain_kv::view>(*session, bytes{0x70});
  };
  reload();

  CHECK(get_matching(*view, 0x1234) == (kv_values{}));
  CHECK(get_matching(*view, 0x1234) == get_matching2(*view, 0x1234));
  CHECK(get_matching(*view, 0x5678) == (kv_values{}));
  CHECK(get_matching(*view, 0x5678) == get_matching2(*view, 0x5678));
  CHECK(get_matching(*view, 0x9abc) == (kv_values{}));
  CHECK(get_matching(*view, 0x9abc) == get_matching2(*view, 0x9abc));

  view->set(0x1234, to_slice({0x30, 0x40}), to_slice({0x50, 0x60}));
  view->set(0x5678, to_slice({0x30, 0x71}), to_slice({0x59, 0x69}));
  view->set(0x5678, to_slice({0x30, 0x00}), to_slice({0x59, 0x69}));
  view->set(0x5678, to_slice({0x30, 0x42}), to_slice({0x55, 0x66}));
  view->set(0x5678, to_slice({0x30, 0x41}), to_slice({0x51, 0x61}));
  view->set(0x9abc, to_slice({0x30, 0x42}), to_slice({0x52, 0x62}));
  reload();

  CHECK(get_matching(*view, 0x1234) ==
    (kv_values{{
      {{0x30, 0x40}, {0x50, 0x60}},
    }}));
  CHECK(get_matching(*view, 0x1234) == get_matching2(*view, 0x1234));
  CHECK(get_matching(*view, 0x5678) ==
    (kv_values{{
      {{0x30, 0x00}, {0x59, 0x69}},
      {{0x30, 0x41}, {0x51, 0x61}},
      {{0x30, 0x42}, {0x55, 0x66}},
      {{0x30, 0x71}, {0x59, 0x69}},
    }}));
  CHECK(get_matching(*view, 0x5678) == get_matching2(*view, 0x5678));
  CHECK(get_matching(*view, 0x9abc) ==
    (kv_values{{
      {{0x30, 0x42}, {0x52, 0x62}},
    }}));
  CHECK(get_matching(*view, 0x9abc) == get_matching2(*view, 0x9abc));

  view->erase(0x5678, to_slice({0x30, 0x00}));
  view->erase(0x5678, to_slice({0x30, 0x71}));
  view->erase(0x5678, to_slice({0x30, 0x42}));
  reload();

  CHECK(get_matching(*view, 0x1234) ==
    (kv_values{{
      {{0x30, 0x40}, {0x50, 0x60}},
    }}));
  CHECK(get_matching(*view, 0x1234) == get_matching2(*view, 0x1234));
  CHECK(get_matching(*view, 0x5678) ==
    (kv_values{{
      {{0x30, 0x41}, {0x51, 0x61}},
    }}));
  CHECK(get_matching(*view, 0x5678) == get_matching2(*view, 0x5678));
  CHECK(get_matching(*view, 0x9abc) ==
    (kv_values{{
      {{0x30, 0x42}, {0x52, 0x62}},
    }}));
  CHECK(get_matching(*view, 0x9abc) == get_matching2(*view, 0x9abc));

  {
    noir::db::chain_kv::view::iterator it{*view, 0x5678, {}};
    it.lower_bound({0x30, 0x22});
    CHECK(get_it(it) ==
      (kv_values{{
        {{0x30, 0x41}, {0x51, 0x61}},
      }}));
    view->set(0x5678, to_slice({0x30, 0x22}), to_slice({0x55, 0x66}));
    CHECK(get_it(it) ==
      (kv_values{{
        {{0x30, 0x41}, {0x51, 0x61}},
      }}));
    it.lower_bound({0x30, 0x22});
    CHECK(get_it(it) ==
      (kv_values{{
        {{0x30, 0x22}, {0x55, 0x66}},
      }}));
    view->set(0x5678, to_slice({0x30, 0x22}), to_slice({0x00, 0x11}));
    CHECK(get_it(it) ==
      (kv_values{{
        {{0x30, 0x22}, {0x00, 0x11}},
      }}));
    view->erase(0x5678, to_slice({0x30, 0x22}));
    KV_REQUIRE_EXCEPTION(it.get_kv(), "kv iterator is at an erased value");
    KV_REQUIRE_EXCEPTION(--it, "kv iterator is at an erased value");
    KV_REQUIRE_EXCEPTION(++it, "kv iterator is at an erased value");
    view->set(0x5678, to_slice({0x30, 0x22}), to_slice({}));
    KV_REQUIRE_EXCEPTION(it.get_kv(), "kv iterator is at an erased value");
    it.lower_bound({0x30, 0x22});
    CHECK(get_it(it) ==
      (kv_values{{
        {{0x30, 0x22}, {}},
      }}));
    view->set(0x5678, to_slice({0x30, 0x22}), to_slice({0x00}));
    CHECK(get_it(it) ==
      (kv_values{{
        {{0x30, 0x22}, {0x00}},
      }}));
    view->erase(0x5678, to_slice({0x30, 0x22}));
    KV_REQUIRE_EXCEPTION(it.get_kv(), "kv iterator is at an erased value");
  }
  reload();

  {
    noir::db::chain_kv::view::iterator it{*view, 0xbeefbeef, {}};
    view->set(0xbeefbeef, to_slice({(char)0x80}), to_slice({(char)0xff}));
    view->set(0xbeefbeef, to_slice({(char)0x90}), to_slice({(char)0xfe}));
    view->set(0xbeefbeef, to_slice({(char)0xa0}), to_slice({(char)0xfd}));
    view->set(0xbeefbeef, to_slice({(char)0xb0}), to_slice({(char)0xfc}));
    it.lower_bound({});
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0x80}, {(char)0xff}},
      }}));
    it.lower_bound({(char)0x80});
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0x80}, {(char)0xff}},
      }}));
    it.lower_bound({(char)0x81});
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0x90}, {(char)0xfe}},
      }}));
    it.lower_bound({(char)0x90});
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0x90}, {(char)0xfe}},
      }}));
    --it;
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0x80}, {(char)0xff}},
      }}));
    ++it;
    ++it;
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0xa0}, {(char)0xfd}},
      }}));
    view->erase(0xbeefbeef, to_slice({(char)0x90}));
    --it;
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0x80}, {(char)0xff}},
      }}));
    view->erase(0xbeefbeef, to_slice({(char)0xa0}));
    ++it;
    CHECK(get_it(it) ==
      (kv_values{{
        {{(char)0xb0}, {(char)0xfc}},
      }}));
  }
  reload();

  CHECK(get_matching(*view, 0xbeefbeef) ==
    (kv_values{{
      {{(char)0x80}, {(char)0xff}},
      {{(char)0xb0}, {(char)0xfc}},
    }}));

  view->set(0xf00df00d, to_slice({0x10, 0x20, 0x00}), to_slice({(char)0x70}));
  view->set(0xf00df00d, to_slice({0x10, 0x20, 0x01}), to_slice({(char)0x71}));
  view->set(0xf00df00d, to_slice({0x10, 0x20, 0x02}), to_slice({(char)0x72}));

  view->set(0xf00df00d, to_slice({0x10, 0x30, 0x00}), to_slice({(char)0x70}));
  view->set(0xf00df00d, to_slice({0x10, 0x30, 0x01}), to_slice({(char)0x71}));
  view->set(0xf00df00d, to_slice({0x10, 0x30, 0x02}), to_slice({(char)0x72}));

  view->set(0xf00df00d, to_slice({0x20, 0x00}), to_slice({(char)0x70}));
  view->set(0xf00df00d, to_slice({0x20, 0x01}), to_slice({(char)0x71}));
  view->set(0xf00df00d, to_slice({0x20, 0x02}), to_slice({(char)0x72}));
  reload();

  CHECK(get_matching(*view, 0xf00df00d, {0x10, 0x20}) ==
    (kv_values{{
      {{0x10, 0x20, 0x00}, {0x70}},
      {{0x10, 0x20, 0x01}, {0x71}},
      {{0x10, 0x20, 0x02}, {0x72}},
    }}));
  CHECK(get_matching(*view, 0xf00df00d, {0x10, 0x20}) == get_matching2(*view, 0xf00df00d, {0x10, 0x20}));

  CHECK(get_matching(*view, 0xf00df00d, {0x10, 0x30}) ==
    (kv_values{{
      {{0x10, 0x30, 0x00}, {0x70}},
      {{0x10, 0x30, 0x01}, {0x71}},
      {{0x10, 0x30, 0x02}, {0x72}},
    }}));
  CHECK(get_matching(*view, 0xf00df00d, {0x10, 0x30}) == get_matching2(*view, 0xf00df00d, {0x10, 0x30}));

  CHECK(get_matching(*view, 0xf00df00d, {0x10}) ==
    (kv_values{{
      {{0x10, 0x20, 0x00}, {0x70}},
      {{0x10, 0x20, 0x01}, {0x71}},
      {{0x10, 0x20, 0x02}, {0x72}},
      {{0x10, 0x30, 0x00}, {0x70}},
      {{0x10, 0x30, 0x01}, {0x71}},
      {{0x10, 0x30, 0x02}, {0x72}},
    }}));
  CHECK(get_matching(*view, 0xf00df00d, {0x10}) == get_matching2(*view, 0xf00df00d, {0x10}));

  CHECK(get_matching(*view, 0xf00df00d, {0x20}) ==
    (kv_values{{
      {{0x20, 0x00}, {0x70}},
      {{0x20, 0x01}, {0x71}},
      {{0x20, 0x02}, {0x72}},
    }}));
  CHECK(get_matching(*view, 0xf00df00d, {0x20}) == get_matching2(*view, 0xf00df00d, {0x20}));
} // view_test()

TEST_CASE("test_view", "view_tests") {
  view_test(false);
  view_test(true);
}
