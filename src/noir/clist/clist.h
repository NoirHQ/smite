// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <eo/sync.h>
#include <shared_mutex>

namespace noir::clist {

using namespace eo;

constexpr auto max_length = std::numeric_limits<int64_t>::max();

inline auto wait_group_1() -> sync::WaitGroupPtr {
  auto wg = std::make_shared<sync::WaitGroup>();
  wg->add(1);
  return wg;
}

template<typename T>
struct CElement;

template<typename T>
using CElementPtr = std::shared_ptr<CElement<T>>;

template<typename T>
class CElement {
private:
  std::shared_mutex mtx;
  CElementPtr<T> prev_;
  sync::WaitGroupPtr prev_wg;
  chan<> prev_wait_ch = make_chan();
  CElementPtr<T> next_;
  sync::WaitGroupPtr next_wg;
  chan<> next_wait_ch = make_chan();
  bool removed_;

public:
  T value;

  CElement() = default;

  template<typename U>
  CElement(U&& v) {
    prev_ = nullptr;
    prev_wg = wait_group_1();
    next_ = nullptr;
    next_wg = wait_group_1();
    removed_ = false;
    value = std::forward<U>(v);
  }

  auto next_wait() -> CElementPtr<T> {
    for (;;) {
      std::shared_lock g{mtx};
      auto next = this->next_;
      auto next_wg = this->next_wg;
      auto removed = this->removed_;
      g.unlock();

      if (next != nullptr || removed) {
        return next;
      }

      next_wg->wait();
    }
  }

  auto prev_wait() -> CElementPtr<T> {
    for (;;) {
      std::shared_lock g{mtx};
      auto prev = this->prev_;
      auto prev_wg = this->prev_wg;
      auto removed = this->removed_;
      g.unlock();

      if (prev != nullptr || removed) {
        return prev;
      }

      prev_wg->wait();
    }
  }

  auto prev_wait_chan() -> chan<>& {
    std::shared_lock g{mtx};
    return prev_wait_ch;
  }

  auto next_wait_chan() -> chan<>& {
    std::shared_lock g{mtx};
    return next_wait_ch;
  }

  auto next() -> CElementPtr<T> {
    std::shared_lock g{mtx};
    return next_;
  }

  auto prev() -> CElementPtr<T> {
    std::shared_lock g{mtx};
    return prev_;
  }

  auto removed() -> bool {
    std::shared_lock g{mtx};
    return removed_;
  }

  void detach_next() {
    std::unique_lock g{mtx};
    if (!removed_) {
      throw std::runtime_error("detach_next() must be called after remove(e)");
    }
    next_ = nullptr;
  }

  void detach_prev() {
    std::unique_lock g{mtx};
    if (!removed_) {
      throw std::runtime_error("detach_prev() must be called after remove(e)");
    }
    prev_ = nullptr;
  }

  void set_next(const CElementPtr<T>& new_next) {
    std::unique_lock g{mtx};

    auto old_next = next_;
    next_ = new_next;
    if (old_next != nullptr && new_next == nullptr) {
      next_wg = wait_group_1();
      next_wait_ch = make_chan();
    }
    if (old_next == nullptr && new_next != nullptr) {
      next_wg->done();
      next_wait_ch.close();
    }
  }

  void set_next(CElementPtr<T>&& new_next) {
    std::unique_lock g{mtx};

    auto old_next = next_;
    next_ = std::move(new_next);
    if (old_next != nullptr && new_next == nullptr) {
      next_wg = wait_group_1();
      next_wait_ch = make_chan();
    }
    if (old_next == nullptr && new_next != nullptr) {
      next_wg->done();
      next_wait_ch.close();
    }
  }

  void set_prev(const CElementPtr<T>& new_prev) {
    std::unique_lock g{mtx};

    auto old_prev = prev_;
    prev_ = new_prev;
    if (old_prev != nullptr && new_prev == nullptr) {
      prev_wg = wait_group_1();
      prev_wait_ch = make_chan();
    }
    if (old_prev == nullptr && new_prev != nullptr) {
      prev_wg->done();
      prev_wait_ch.close();
    }
  }

  void set_prev(CElementPtr<T>&& new_prev) {
    std::unique_lock g{mtx};

    auto old_prev = prev_;
    prev_ = std::move(new_prev);
    if (old_prev != nullptr && new_prev == nullptr) {
      prev_wg = wait_group_1();
      prev_wait_ch = make_chan();
    }
    if (old_prev == nullptr && new_prev != nullptr) {
      prev_wg->done();
      prev_wait_ch.close();
    }
  }

  void set_removed() {
    std::unique_lock g{mtx};

    removed_ = true;

    if (prev_ == nullptr) {
      prev_wg->done();
      prev_wait_ch.close();
    }
    if (next_ == nullptr) {
      next_wg->done();
      next_wait_ch.close();
    }
  }
};

template<typename T>
class CList {
private:
  std::shared_mutex mtx;
  sync::WaitGroupPtr wg = wait_group_1();
  chan<> wait_ch = make_chan();
  CElementPtr<T> head{};
  CElementPtr<T> tail{};
  int64_t len_ = 0;
  int64_t max_len = max_length;

public:
  CList() = default;
  CList(int64_t max_length): max_len(max_length) {}

  auto len() -> int64_t {
    std::shared_lock g{mtx};
    return len_;
  }

  auto front() -> CElementPtr<T> {
    std::shared_lock g{mtx};
    return head;
  }

  auto front_wait() -> CElementPtr<T> {
    for (;;) {
      std::shared_lock g{mtx};
      auto head = this->head;
      auto wg = this->wg;
      g.unlock();

      if (head != nullptr) {
        return head;
      }
      wg->wait();
    }
  }

  auto back() -> CElementPtr<T> {
    std::shared_lock g{mtx};
    return tail;
  }

  auto back_wait() -> CElementPtr<T> {
    for (;;) {
      std::shared_lock g{mtx};
      auto tail = this->tail;
      auto wg = this->wg;
      g.unlock();

      if (tail != nullptr) {
        return tail;
      }
      wg->wait();
    }
  }

  auto wait_chan() -> chan<>& {
    std::unique_lock g{mtx};
    return wait_ch;
  }

  template<typename U>
  auto push_back(U&& v) -> CElementPtr<T> {
    std::unique_lock g{mtx};

    auto e = std::make_shared<CElement<T>>(std::forward<U>(v));

    if (len_ == 0) {
      wg->done();
      wait_ch.close();
    }
    if (len_ >= max_len) {
      throw std::runtime_error(fmt::format("clist: maximum length list reached {:d}", max_len));
    }
    len_++;

    if (tail == nullptr) {
      head = e;
      tail = e;
    } else {
      e->set_prev(tail);
      tail->set_next(e);
      tail = e;
    }
    return e;
  }

  auto remove(const CElementPtr<T>& e) -> T {
    std::unique_lock g{mtx};

    auto prev = e->prev();
    auto next = e->next();

    if (head == nullptr || tail == nullptr) {
      throw std::runtime_error("Remove(e) on empty CList");
    }
    if (prev == nullptr && head != e) {
      throw std::runtime_error("Remove(e) with false head");
    }
    if (next == nullptr && tail != e) {
      throw std::runtime_error("Remove(e) with false tail");
    }

    if (len_ == 1) {
      wg = wait_group_1();
      wait_ch = make_chan();
    }

    len_--;

    if (prev == nullptr) {
      head = next;
    } else {
      prev->set_next(next);
    }
    if (next == nullptr) {
      tail = prev;
    } else {
      next->set_prev(prev);
    }

    e->set_removed();

    return e->value;
  }
};

} // namespace noir::clist
