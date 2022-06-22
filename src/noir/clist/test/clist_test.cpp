// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/clist/clist.h>
#include <eo/fmt.h>
#include <eo/math/rand.h>
#include <eo/time.h>
#include <iostream>
#include <thread>

#define ASSERT_MESSAGE(assertion, cond, msg) \
  do { \
    INFO(msg); \
    assertion(cond); \
  } while (0)

#define CHECK_MESSAGE(cond, msg) ASSERT_MESSAGE(CHECK, cond, msg)
#define REQUIRE_MESSAGE(cond, msg) ASSERT_MESSAGE(REQUIRE, cond, msg)

using namespace noir;
using namespace noir::clist;
namespace mrand = eo::math::rand;

TEST_CASE("clist", "[noir][clist]") {
  SECTION("throwing an exception on max length") {
    auto max_length = 1000;

    auto l = CList<int>(max_length);
    for (auto i = 0; i < max_length; i++) {
      l.push_back(i);
    }
    CHECK_THROWS_WITH(l.push_back(1), fmt::format("clist: maximum length list reached {}", max_length));
  }

  SECTION("small") {
    auto l = CList<int>();
    auto e1 = l.push_back(1);
    auto e2 = l.push_back(2);
    auto e3 = l.push_back(3);
    CHECK_MESSAGE(l.len() == 3, fmt::format("Expected len 3, got {}", l.len()));

    auto r1 = l.remove(e1);
    auto r2 = l.remove(e2);
    auto r3 = l.remove(e3);
    CHECK_MESSAGE(r1 == 1, fmt::format("Expected 1, got {}", r1));
    CHECK_MESSAGE(r2 == 2, fmt::format("Expected 1, got {}", r2));
    CHECK_MESSAGE(r3 == 3, fmt::format("Expected 1, got {}", r3));
    CHECK_MESSAGE(l.len() == 0, fmt::format("Expected len 0, got {}", l.len()));
  }

  SECTION("scan right delete random") {
    int num_elements = 1000;
    int num_times = 100;
    int num_scanners = 10;

    auto l = CList<int>();
    auto stop = make_chan();

    std::vector<CElementPtr<int>> els(num_elements);
    for (auto i = 0; i < num_elements; i++) {
      auto el = l.push_back(i);
      els[i] = el;
    }

    for (auto i = 0; i < num_scanners; i++) {
      go([&, scanner_id(i)]() -> func<> {
        CElementPtr<int> el{};
        auto restart_counter = 0;
        auto counter = 0;

        auto loop = true;
        while (loop) {
          auto res = co_await (*stop || *default_chan);
          switch (res.index()) {
          case 0:
            fmt::println("stopped");
            loop = false;
            continue;
          case 1:
            break;
          }
          if (!el) {
            el = l.front_wait();
            restart_counter++;
          }
          el = el->next();
          counter++;
        }
        fmt::print("Scanner {} restart_counter: {} counter: {}\n", scanner_id, restart_counter, counter);
      });
    }

    for (auto i = 0; i < num_times; i++) {
      auto rm_el_idx = mrand::int_n(els.size());
      auto rm_el = els[rm_el_idx];

      l.remove(rm_el);

      auto new_el = l.push_back(-1 * i - 1);
      els[rm_el_idx] = new_el;

      if (i % 100000 == 0) {
        fmt::print("Pushed {}K elements so far...\n", i / 1000);
      }
    }

    stop.close();

    for (auto el = l.front(); el; el = el->next()) {
      l.remove(el);
    }
    REQUIRE_MESSAGE(l.len() == 0, "Failed to remove all elements from CList");

    runtime::executor.join();
  }

  SECTION("wait_chan") {
    go([]() -> func<> {
      auto l = CList<int>();
      auto& ch = l.wait_chan();

      go([&]() { l.push_back(1); });
      co_await *ch;

      auto el = l.front();
      auto v = l.remove(el);
      REQUIRE_MESSAGE(v == 1, "where is 1 coming from?");

      el = l.push_back(0);
      auto done = make_chan();
      auto pushed = 0;
      go([&]() -> func<> {
        for (auto i = 0; i < 100; i++) {
          l.push_back(i);
          pushed++;
          co_await time::sleep(std::chrono::milliseconds(math::rand::int_n(25)));
        }
        co_await time::sleep(std::chrono::milliseconds(25));
        done.close();
      });

      auto next = el;
      auto seen = 0;

      for (auto loop = true; loop;) {
        auto res = co_await (*next->next_wait_chan() || *done || time::sleep(std::chrono::seconds(10)));
        switch (res.index()) {
        case 0:
          next = next->next();
          seen++;
          REQUIRE_MESSAGE(next, "next should not be null when waiting on next_wait_chan");
          break;
        case 1:
          loop = false;
          break;
        case 2:
          REQUIRE_MESSAGE(false, "max execution time");
          break;
        }
      }

      REQUIRE_MESSAGE(pushed == seen,
        fmt::format("number of pushed items ({:d}) not equal to number of seen items ({:d})", pushed, seen));

      auto prev = next;
      seen = 0;

      for (auto loop = true; loop;) {
        auto res = co_await (*prev->prev_wait_chan() || time::sleep(std::chrono::seconds(3)));
        switch (res.index()) {
        case 0:
          prev = prev->prev();
          seen++;
          REQUIRE_MESSAGE(prev, "expected prev_wait_chan to block forever on null when reached first elem");
          break;
        case 1:
          loop = false;
          break;
        }
      }

      REQUIRE_MESSAGE(pushed == seen,
        fmt::format("number of pushed items ({:d}) not equal to number of seen items ({:d})", pushed, seen));
    });

    runtime::executor.join();
  }

  SECTION("removed") {
    auto l = clist::CList<int>();
    auto el1 = l.push_back(1);
    auto el2 = l.push_back(2);
    l.remove(el1);
    CHECK(el1->removed());
    CHECK(!el2->removed());
  }

  SECTION("next_wait_chan") {
    go([]() -> func<> {
      auto l = clist::CList<int>();
      auto el1 = l.push_back(1);
      {
        auto res = co_await (*el1->next_wait_chan() || *default_chan);
        switch (res.index()) {
        case 0:
          REQUIRE_MESSAGE(false, "next_wait_chan should not have been closed");
          break;
        case 1:
          break;
        }
      }

      auto el2 = l.push_back(2);
      {
        auto res1 = co_await (*el1->next_wait_chan() || *default_chan);
        switch (res1.index()) {
        case 0:
          CHECK(el1->next());
          break;
        case 1:
          REQUIRE_MESSAGE(false, "next_wait_chan should have been closed");
          break;
        }

        auto res2 = co_await (*el2->next_wait_chan() || *default_chan);
        switch (res2.index()) {
        case 0:
          REQUIRE_MESSAGE(false, "next_wait_chan should not have been closed");
          break;
        case 1:
          break;
        }
      }

      {
        l.remove(el2);
        auto res = co_await (*el2->next_wait_chan() || *default_chan);
        switch (res.index()) {
        case 0:
          CHECK(!el2->next());
          break;
        case 1:
          REQUIRE_MESSAGE(false, "next_wait_chan should have been closed");
          break;
        }
      }
    });
    runtime::executor.join();
  }
}
