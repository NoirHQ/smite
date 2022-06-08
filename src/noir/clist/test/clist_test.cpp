// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/clist/clist.h>
#include <thread>

using namespace noir;

TEST_CASE("clist: max_length", "[noir][clist]") {
  int max_length = 1000;
  auto l = clist<int>::new_clist(max_length);
  for (auto i = 0; i < max_length; i++)
    l->push_back(i);
  CHECK_THROWS_WITH(l->push_back(1), fmt::format("clist: maximum length reached {}", max_length));
}

TEST_CASE("clist: small", "[noir][clist]") {
  auto l = clist<int>::new_clist();
  auto e1 = l->push_back(1);
  auto e2 = l->push_back(2);
  auto e3 = l->push_back(3);
  CHECK(l->get_len() == 3);
  l->remove(e1);
  l->remove(e2);
  l->remove(e3);
  CHECK(l->get_len() == 0);
}

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
