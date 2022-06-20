// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/clist/clist.h>
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
}

/*
TEST_CASE("clist: scan right delete random", "[noir][clist]") {
  int num_elements = 1000;
  int num_times = 100;
  int num_scanners = 10;

  auto l = clist<int>::new_clist();
  bool stop{};

  std::vector<e_ptr<int>> els(num_elements);
  for (auto i = 0; i < num_elements; i++) {
    auto el = l->push_back(i);
    els[i] = el;
  }

  std::vector<std::thread> thrds(num_scanners);
  std::mutex mtx;
  for (auto i = 0; i < num_scanners; i++) {
    thrds[i] = std::thread([i, &stop, &l, &mtx]() {
      e_ptr<int> el{};
      int restart_counter{}, counter{};
      for (;;) {
        if (stop) {
          std::scoped_lock _(mtx);
          std::cout << "stopped\n";
          std::cout << fmt::format("scanner: {} restart_counter: {} counter: {}\n", i, restart_counter, counter);
          return;
        }
        if (!el) {
          el = l->front_wait();
          restart_counter++;
        }
        el = el->get_next();
        counter++;
      }
    });
  }

  for (auto i = 0; i < num_times; i++) {
    int rm_el_idx = rand() % num_elements;
    auto rm_el = els[rm_el_idx];
    l->remove(rm_el);
    auto new_el = l->push_back(1);
    els[rm_el_idx] = new_el;
    if (i % 100000 == 0)
      std::cout << fmt::format("pushed #{}/1000K so far\n", i);
  }

  stop = true;
  for (auto& t : thrds)
    t.join();

  for (auto el = l->front(); el; el = el->get_next())
    l->remove(el);
  CHECK(l->get_len() == 0);
}

TEST_CASE("clist: wait", "[noir][clist]") {
  auto l = clist<int>::new_clist();
  l->push_back(1);

  auto el = l->front();
  l->remove(el);
  CHECK(l->get_len() == 0);

  el = l->push_back(0);
  int pushed{};
  bool done{};
  auto thrd = std::thread([&]() {
    for (auto i = 1; i < 100; i++) {
      l->push_back(i);
      pushed++;
      std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{25});
    done = true;
  });

  auto next = el;
  int seen{};
  for (;;) {
    next = next->next_wait_with_timeout(std::chrono::milliseconds{100});
    if (next)
      seen++;
    if (done)
      break;
  }
  thrd.join();

  CHECK(pushed == seen);
}

TEST_CASE("clist: removed", "[noir][clist]") {
  auto l = clist<int>::new_clist();
  auto el1 = l->push_back(1);
  auto el2 = l->push_back(2);
  l->remove(el1);
  CHECK(el1->get_removed());
  CHECK(!el2->get_removed());
}

TEST_CASE("clist: next wait", "[noir][clist]") {
  auto l = clist<int>::new_clist();
  auto el1 = l->push_back(1);
  CHECK(el1->next_wait_with_timeout(std::chrono::milliseconds{1}) == nullptr);

  auto el2 = l->push_back(2);
  CHECK(el1->next_wait_with_timeout(std::chrono::milliseconds{1}) != nullptr);
  CHECK(el2->next_wait_with_timeout(std::chrono::milliseconds{1}) == nullptr);

  l->remove(el2);
  CHECK(el2->next_wait_with_timeout(std::chrono::milliseconds{1}) == nullptr);
}
*/

TEST_CASE("clist: WaitChan", "[noir][clist]") {
  go([]() -> func<void> {
    auto l = CList<int>();
    auto& ch = l.wait_chan();

    go([&]() { l.push_back(1); });
    co_await ch.async_receive(eoroutine);

    auto el = l.front();
    auto v = l.remove(el);
    REQUIRE_MESSAGE(v == 1, "where is 1 coming from?");

    el = l.push_back(0);
    auto done = make_chan();
    auto pushed = 0;
    go([&]() -> func<void> {
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
      auto res = co_await(next->next_wait_chan().async_receive(eoroutine) || done.async_receive(eoroutine) ||
        boost::asio::steady_timer{executor, std::chrono::seconds(10)}.async_wait(eoroutine));
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
      auto res = co_await(prev->prev_wait_chan().async_receive(eoroutine) ||
        boost::asio::steady_timer{executor, std::chrono::seconds(3)}.async_wait(eoroutine));
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

  executor.join();
}
