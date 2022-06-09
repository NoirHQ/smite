// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>

namespace noir {

constexpr int clist_max_length{std::numeric_limits<int>::max()};

template<typename T>
struct c_element;
template<typename T>
using e_ptr = std::shared_ptr<c_element<T>>;

template<typename T>
struct c_element {
private:
  std::mutex mtx;
  std::condition_variable cv_next;
  e_ptr<T> prev{};
  e_ptr<T> next{};
  bool removed{};

public:
  T value;

  explicit c_element(T v): value(v) {}

  static e_ptr<T> new_e(T v) {
    auto ret = std::make_shared<c_element<T>>(v);
    return ret;
  }

  /// \brief blocks until next is available or got removed; note may return nullptr
  e_ptr<T> next_wait() {
    std::unique_lock<std::mutex> lck(mtx);
    cv_next.wait(lck, [this] { return (next != nullptr || removed); });
    return next;
  }

  e_ptr<T> next_wait_with_timeout(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lck(mtx);
    cv_next.wait_for(lck, timeout, [this] { return (next != nullptr || removed); });
    return next;
  }

  e_ptr<T> get_next() {
    std::scoped_lock _(mtx);
    return next;
  }

  e_ptr<T> get_prev() {
    std::scoped_lock _(mtx);
    return prev;
  }

  bool get_removed() {
    std::scoped_lock _(mtx);
    return removed;
  }

  void detach_next() {
    std::scoped_lock _(mtx);
    check(removed, "detach_next() must be called after remove");
    next.reset();
  }

  void detach_prev() {
    std::scoped_lock _(mtx);
    check(removed, "detach_prev() must be called after remove");
    prev.reset();
  }

  void set_next(e_ptr<T> new_next) {
    std::scoped_lock _(mtx);
    auto old_next = next;
    next = new_next;
    // if (old_next && !new_next) {
    // }
    if (!old_next && new_next) {
      cv_next.notify_all();
    }
  }

  void set_prev(e_ptr<T> new_prev) {
    std::scoped_lock _(mtx);
    auto old_prev = prev;
    prev = new_prev;
    // if (!old_prev && new_prev) {
    //   cv_prev.notify_all();
    // }
  }

  void set_removed() {
    std::scoped_lock _(mtx);
    removed = true;
    if (!next)
      cv_next.notify_all();
  }
};

template<typename T>
struct clist {
private:
  std::mutex mtx;
  std::condition_variable cv;
  e_ptr<T> head{};
  e_ptr<T> tail{};
  int len{};
  int max_len{};

public:
  clist() {}

  static std::unique_ptr<clist<T>> new_clist(int max_length = clist_max_length) {
    auto ret = std::make_unique<clist<T>>();
    ret->max_len = max_length;
    return ret;
  }

  int get_len() {
    std::scoped_lock _(mtx);
    return len;
  }

  e_ptr<T> front() {
    std::scoped_lock _(mtx);
    return head;
  }

  /// \brief blocks until front is available
  e_ptr<T> front_wait() {
    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, [this] { return head != nullptr; });
    return head;
  }

  e_ptr<T> back() {
    std::scoped_lock _(mtx);
    return tail;
  }

  /// \brief blocks until back is available
  e_ptr<T> back_wait() {
    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck, !tail);
    return tail;
  }

  e_ptr<T> push_back(T v) {
    std::scoped_lock _(mtx);
    auto e = c_element<T>::new_e(v);
    check(len < max_len, fmt::format("clist: maximum length reached {}", max_len));
    if (tail == nullptr) {
      head = e;
      tail = e;
    } else {
      e->set_prev(tail);
      tail->set_next(e);
      tail = e;
    }
    if (len == 0)
      cv.notify_all();
    len++;
    return e;
  }

  void remove(e_ptr<T> e) {
    std::scoped_lock _(mtx);
    auto prev = e->get_prev();
    auto next = e->get_next();
    check(head && tail, "remove on empty clist");
    check(prev || head == e, "remove with false head");
    check(next || tail == e, "remove with false tail");
    len--;
    if (!prev)
      head = next;
    else
      prev->set_next(next);
    if (!next)
      tail = prev;
    else
      next->set_prev(prev);
    e->set_removed();
  }
};

} // namespace noir
