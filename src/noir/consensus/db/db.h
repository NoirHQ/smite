// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/p2p/types.h>

namespace noir::consensus {

class db {
public:
  virtual ~db() {};
  virtual bool get(const noir::p2p::bytes& key, noir::p2p::bytes& val) const = 0;
  virtual bool has(const noir::p2p::bytes& key, bool& val) const = 0;
  virtual bool set(const noir::p2p::bytes& key, const noir::p2p::bytes& val) = 0;
  virtual bool set_sync(const noir::p2p::bytes& key, const noir::p2p::bytes& val) = 0;
  virtual bool del(const noir::p2p::bytes& key) = 0;
  virtual bool del_sync(const noir::p2p::bytes& key) = 0;
  virtual bool close() = 0;
  virtual bool print() const = 0;
  virtual const std::map<noir::p2p::bytes, noir::p2p::bytes>& stats() = 0;

  class batch {
  public:
    virtual ~batch() {};
    virtual bool set(const noir::p2p::bytes& key, const noir::p2p::bytes& val) = 0;
    virtual bool del(const noir::p2p::bytes& key) = 0;
    virtual bool write() = 0;
    virtual bool write_sync() = 0;
    virtual bool close() = 0;

  };

  virtual std::shared_ptr<batch> new_batch() = 0;
};

class simple_db : public db {
private:
  class simple_db_impl {
  public:
    bool get(const noir::p2p::bytes& key, noir::p2p::bytes& val) const;
    bool has(const noir::p2p::bytes& key, bool& val) const;
    bool set(const noir::p2p::bytes& key, const noir::p2p::bytes& val);
    bool set_sync(const noir::p2p::bytes& key, const noir::p2p::bytes& val);
    bool del(const noir::p2p::bytes& key);
    bool del_sync(const noir::p2p::bytes& key);
    bool close();
    bool print() const;
    const std::map<noir::p2p::bytes, noir::p2p::bytes>& stats();

  private:
    std::map<noir::p2p::bytes, noir::p2p::bytes> _map;
  };

  std::shared_ptr<simple_db_impl> _impl;

public:
  simple_db() : _impl(new simple_db_impl) {}

  simple_db(const simple_db& other) : _impl(std::make_shared<simple_db_impl>(*other._impl)) {}

  simple_db(simple_db&& other) : _impl(std::move(other._impl)) { other._impl = nullptr; }

  ~simple_db() override {}

  bool get(const noir::p2p::bytes& key, noir::p2p::bytes& val) const override;
  bool has(const noir::p2p::bytes& key, bool& val) const override;
  bool set(const noir::p2p::bytes& key, const noir::p2p::bytes& val) override;
  bool set_sync(const noir::p2p::bytes& key, const noir::p2p::bytes& val) override;
  bool del(const noir::p2p::bytes& key) override;
  bool del_sync(const noir::p2p::bytes& key) override;
  bool close() override;
  bool print() const override;
  const std::map<noir::p2p::bytes, noir::p2p::bytes>& stats() override;

  class simple_db_batch : public batch {
  public:
    explicit simple_db_batch(std::shared_ptr<simple_db_impl> db_impl) : _db(std::move(db_impl)) {}

    explicit simple_db_batch(const simple_db_batch& other) noexcept: _db(other._db), _map(other._map) {}

    explicit simple_db_batch(simple_db_batch&& other) noexcept: _db(std::move(other._db)) { other._db = nullptr; }

    ~simple_db_batch() override {};

    bool set(const noir::p2p::bytes& key, const noir::p2p::bytes& val) override;
    bool del(const noir::p2p::bytes& key) override;
    bool write() override;
    bool write_sync() override;
    bool close() override;
  private:
    std::shared_ptr<simple_db_impl> _db;
    std::map<noir::p2p::bytes, std::pair<bool, noir::p2p::bytes>> _map;
    bool _is_closed = false;
  };

  std::shared_ptr<batch> new_batch() override {
    return std::make_shared<simple_db_batch>(simple_db_batch(_impl));
  }

};

} // noir::consensus
