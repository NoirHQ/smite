// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/scale.h>
#include <noir/common/hex.h>
#include <noir/common/thread_pool.h>
#include <noir/tx_pool/LRU_cache.h>
#include <noir/tx_pool/tx_pool.h>
#include <noir/tx_pool/unapplied_tx_queue.h>
#include <algorithm>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;

appbase::application app;

namespace test_detail {

static address_type str_to_addr(const std::string& str) {
  address_type addr(str.begin(), str.end());
  return addr;
}

std::random_device random_device_;
std::mt19937 generator{random_device_()};

uint64_t rand_gas(uint64_t max = 0xFFFF, uint64_t min = 0) {
  std::uniform_int_distribution<uint64_t> dist_gas{min, max};
  return dist_gas(generator);
}

class test_application : public application::base_application {
  std::shared_mutex mutex_;
  std::list<std::shared_ptr<req_res<response_check_tx>>> rrs_;
  uint64_t nonce_ = 0;
  uint64_t gas_wanted_ = 0;

  std::unique_ptr<named_thread_pool> thread_;
  bool is_running = false;

public:
  test_application() {
    thread_ = std::make_unique<named_thread_pool>("test_application_thread", 1);
    is_running = true;

    async_thread_pool(thread_->get_executor(), [&]() {
      while (is_running) {
        handle_response();
        usleep(10);
      }
    });
  }

  ~test_application() {
    is_running = false;
    thread_->stop();
  }

  req_res<response_check_tx>& new_rr() {
    std::scoped_lock _(mutex_);
    response_check_tx res{
      .gas_wanted = gas_wanted_,
      .sender = str_to_addr("user"),
      .nonce = nonce_++,
    };

    auto rr_ptr = std::make_shared<req_res<response_check_tx>>();
    rr_ptr->res = std::make_shared<response_check_tx>(res);
    rrs_.push_back(rr_ptr);

    return *rr_ptr;
  }

  response_check_tx& check_tx_sync() override {
    return *new_rr().res;
  }

  req_res<response_check_tx>& check_tx_async() override {
    return new_rr();
  }

  void set_nonce(uint64_t nonce) {
    std::scoped_lock _(mutex_);
    nonce_ = nonce;
  }

  void set_gas(uint64_t gas) {
    std::scoped_lock lock(mutex_);
    gas_wanted_ = gas;
  }

  void handle_response() {
    if (rrs_.begin() != rrs_.end()) {
      auto& rr = rrs_.front();
      if (rr->callback != nullptr) {
        rr->invoke_callback();
        std::unique_lock _(mutex_);
        rrs_.pop_front();
      }
    }
  }

  void reset() {
    std::unique_lock _(mutex_);
    rrs_.clear();
    nonce_ = 0;
    gas_wanted_ = 0;
  }

  bool is_empty_response() {
    std::shared_lock _(mutex_);
    return rrs_.empty();
  }
};

class test_helper {
private:
  uint64_t tx_id_ = 0;
  uint64_t nonce_ = 0;
  uint64_t height_ = 0;

  std::mutex mutex_;

  std::shared_ptr<class tx_pool> tp_ = nullptr;

  std::shared_ptr<test_application> test_app_ = nullptr;

public:
  auto new_tx() {
    std::scoped_lock lock(mutex_);
    return codec::scale::encode(tx_id_++);
  }

  auto new_txs(uint count) {
    std::vector<consensus::tx> txs;
    txs.reserve(count);
    for (uint i = 0; i < count; i++) {
      txs.push_back(new_tx());
    }
    return txs;
  }

  auto make_random_wrapped_tx(const std::string& sender) {
    wrapped_tx new_wrapped_tx;
    new_wrapped_tx.sender = str_to_addr(sender);
    new_wrapped_tx.gas = rand_gas();

    std::scoped_lock lock(mutex_);
    new_wrapped_tx.tx = codec::scale::encode(tx_id_++);
    new_wrapped_tx.nonce = nonce_++;
    new_wrapped_tx.height = height_++;
    return std::make_shared<wrapped_tx>(new_wrapped_tx);
  }

  void reset_tx_id() {
    std::scoped_lock lock(mutex_);
    tx_id_ = 0;
  }

  class tx_pool& make_tx_pool() {
    config cfg{};
    test_app_ = std::make_shared<test_application>();
    return make_tx_pool(cfg, test_app_);
  }

  class tx_pool& make_tx_pool(config& cfg) {
    test_app_ = std::make_shared<test_application>();
    return make_tx_pool(cfg, test_app_);
  }

  class tx_pool& make_tx_pool(std::shared_ptr<test_application>& test_app) {
    config cfg{};
    return make_tx_pool(cfg, test_app);
  }

  class tx_pool& make_tx_pool(config& cfg, std::shared_ptr<test_application>& test_app) {
    auto proxy_app = std::make_shared<app_connection>();
    test_app_ = test_app;
    proxy_app->application = test_app;
    tp_ = std::make_shared<noir::tx_pool::tx_pool>(app, cfg, proxy_app, 0);
    return *tp_;
  }
};

} // namespace test_detail

using namespace test_detail;

TEST_CASE("unapplied_tx_queue: Tx queue basic test", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  unapplied_tx_queue tx_queue;

  const uint64_t tx_count = 10;
  wrapped_tx_ptrs wtxs;
  wtxs.reserve(tx_count);
  for (auto i = 0; i < tx_count; i++) {
    wtxs.push_back(test_helper->make_random_wrapped_tx("user"));
  }

  // Add tx
  for (auto& wtx : wtxs) {
    CHECK(tx_queue.add_tx(wtx));
  }
  CHECK(tx_queue.size() == tx_count);

  SECTION("Add same tx hash") { // fail case
    auto wtx = wtxs[0];
    wtx->nonce = wtxs[tx_count - 1]->nonce + 1;
    CHECK(tx_queue.add_tx(wtx));
    CHECK(tx_queue.size() == tx_count);
    CHECK(tx_queue.get_tx(wtxs[0]->hash()));
  }

  SECTION("Has") {
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.has(wtx->hash()));
    }
  }

  SECTION("Get") {
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.get_tx(wtx->hash()));
    }
  }

  SECTION("Erase tx") {
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.erase(wtx->hash()));
    }
    CHECK(tx_queue.empty());

    // fail case
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.erase(wtx->hash()) == false);
    }
    CHECK(tx_queue.empty());
  }

  SECTION("Erase tx by iterator") {
    std::for_each(tx_queue.begin(), tx_queue.end(), [&](auto& itr) { CHECK(tx_queue.erase(itr.hash())); });
    CHECK(tx_queue.empty());
  }

  SECTION("Flush") {
    tx_queue.clear();
    CHECK(tx_queue.empty());
  }
}

TEST_CASE("unapplied_tx_queue: Fully add/erase tx", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  uint64_t tx_count = 10000;
  uint64_t queue_size = 0;
  wrapped_tx_ptrs wtxs;
  wtxs.reserve(tx_count);
  for (uint64_t i = 0; i < tx_count; i++) {
    wtxs.push_back(test_helper->make_random_wrapped_tx("user"));
  }

  for (auto& wtx : wtxs) {
    queue_size += unapplied_tx_queue::bytes_size(wtx);
  }

  auto tx_queue = std::make_unique<unapplied_tx_queue>(queue_size);
  for (auto& wtx : wtxs) {
    CHECK(tx_queue->add_tx(wtx));
  }
  CHECK(tx_queue->size() == tx_count);

  for (auto& wtx : wtxs) {
    CHECK(tx_queue->erase(wtx->hash()));
  }
  CHECK(tx_queue->empty());
}

TEST_CASE("unapplied_tx_queue: Indexing", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  uint64_t tx_count = 10000;
  uint64_t user_count = tx_count / 100;
  uint64_t queue_size = config{}.max_tx_bytes * tx_count;
  auto tx_queue = std::make_unique<unapplied_tx_queue>(queue_size);

  for (uint64_t i = 0; i < tx_count; i++) {
    CHECK(tx_queue->add_tx(test_helper->make_random_wrapped_tx("user" + std::to_string(i / user_count))));
  }
  CHECK(tx_queue->size() == tx_count);

  SECTION("by nonce") {
    auto tx = test_helper->make_random_wrapped_tx("Alice");
    tx_queue->add_tx(tx);
    auto tx_ptr = tx_queue->get_tx(tx->sender, tx->nonce);
    CHECKED_IF(tx_ptr) {
      CHECK(tx_ptr->nonce == tx->nonce);
    }
  }

  SECTION("a specific sender's txs") {
    SECTION("all txs") {
      uint64_t tx_count_per_user = tx_count / user_count;
      for (uint64_t i = 0; i < user_count; i++) {
        address_type sender = str_to_addr("user" + std::to_string(i));
        auto begin = tx_queue->begin(sender);
        auto end = tx_queue->end(sender);

        uint64_t count = 0;
        for (auto itr = begin; itr != end; itr++) {
          count++;
        }
        CHECK(count == tx_count_per_user);
      }
    }
  }

  SECTION("ordering") {
    SECTION("ascending") {
      auto begin = tx_queue->begin<unapplied_tx_queue::by_gas>();
      auto end = tx_queue->end<unapplied_tx_queue::by_gas>();

      uint64_t prev_gas = 0;
      for (auto itr = begin; itr != end; itr++) {
        CHECK(itr->gas() >= prev_gas);
        prev_gas = itr->gas();
      }
    }

    SECTION("descending") {
      auto rbegin = tx_queue->rbegin<unapplied_tx_queue::by_gas>();
      auto rend = tx_queue->rend<unapplied_tx_queue::by_gas>();

      uint64_t prev_gas = std::numeric_limits<uint64_t>::max();
      for (auto itr = rbegin; itr != rend; itr++) {
        CHECK(itr->gas() <= prev_gas);
        prev_gas = itr->gas();
      }
    }
  }

  SECTION("bound") {
    uint64_t lowest = 1000;
    uint64_t highest = 50000;
    auto begin = tx_queue->begin<unapplied_tx_queue::by_gas>(lowest);
    auto end = tx_queue->end<unapplied_tx_queue::by_gas>(highest);

    for (auto itr = begin; itr != end; itr++) {
      CHECK(lowest <= itr->gas());
      CHECK(itr->gas() <= highest);
    }

    auto rbegin = tx_queue->rbegin<unapplied_tx_queue::by_gas>(highest);
    auto rend = tx_queue->rend<unapplied_tx_queue::by_gas>(lowest);

    for (auto itr = rbegin; itr != rend; itr++) {
      CHECK(lowest <= itr->gas());
      CHECK(itr->gas() <= highest);
    }
  }
}

TEST_CASE("tx_pool: Add/Get tx", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  auto test_app = std::make_shared<test_application>();
  auto& tp = test_helper->make_tx_pool(test_app);

  SECTION("sync") {
    uint tx_count = 10;
    auto txs = test_helper->new_txs(tx_count);

    for (auto& tx : txs) {
      CHECK_NOTHROW(tp.check_tx_sync(tx));
    }

    // fail case : same txs
    for (auto& tx : txs) {
      CHECK_THROWS_AS(tp.check_tx_sync(tx), fc::existed_tx_exception);
    }

    CHECK(txs.size() == tx_count);
  }

  SECTION("async") {
    size_t thread_num = 10;
    auto thread = std::make_unique<named_thread_pool>("test_thread", thread_num);
    uint tx_count = 100;

    SECTION("multi thread add") {
      std::atomic<uint> token = thread_num;
      std::future<void> fs[thread_num];
      for (auto i = 0; i < thread_num; i++) {
        fs[i] = async_thread_pool(thread->get_executor(), [&]() {
          auto txs = test_helper->new_txs(tx_count);
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread
          for (auto& tx : txs) {
            CHECK_NOTHROW(tp.check_tx_async(tx));
          }
        });
      }

      for (auto& f : fs) {
        f.get();
      }
      while (!test_app->is_empty_response()) {
      }

      CHECK(tp.size() == tx_count * thread_num);
    }

    SECTION("1 thread add / 1 thread get") {
      std::atomic<uint> token = 2;
      std::future<void> add_result;
      add_result = async_thread_pool(thread->get_executor(), [&]() {
        auto txs = test_helper->new_txs(tx_count);
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread
        for (auto& tx : txs) {
          CHECK_NOTHROW(tp.check_tx_async(tx));
        }
      });

      std::future<uint> get_result;
      get_result = async_thread_pool(thread->get_executor(), [&]() {
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread
        uint get_count = 0;
        auto start_time = get_time();
        while (get_count < tx_count) {
          auto txs = tp.reap_max_txs(tx_count - get_count);
          get_count += txs.size();
          if (get_time() - start_time >= 5000000 /* 5sec */) {
            break;
          }
        }
        return get_count;
      });

      add_result.get();
      while (!test_app->is_empty_response()) {
      }

      CHECK(get_result.get() == tx_count);
    }

    SECTION("2 thread add / 2 thread get") {
      std::atomic<uint> token = 4;
      std::future<void> add_result[2];

      for (auto& res : add_result) {
        res = async_thread_pool(thread->get_executor(), [&]() {
          auto txs = test_helper->new_txs(tx_count);
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread
          for (auto& tx : txs) {
            CHECK_NOTHROW(tp.check_tx_async(tx));
          }
        });
      }

      std::future<uint> get_result[2];
      for (auto& res : get_result) {
        res = async_thread_pool(thread->get_executor(), [&]() {
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread
          uint get_count = 0;
          auto start_time = get_time();
          while (get_count < tx_count) {
            auto txs = tp.reap_max_txs(tx_count - get_count);
            get_count += txs.size();
            if (get_time() - start_time >= 5000000 /* 5sec */) {
              break;
            }
          }
          return get_count;
        });
      }

      for (auto& res : add_result) {
        res.get();
      }
      while (!test_app->is_empty_response()) {
      }

      CHECK(get_result[0].get() + get_result[1].get() == tx_count * 2);
    }
  }
}

TEST_CASE("tx_pool: Reap tx using max bytes & gas", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  auto test_app = std::make_shared<test_application>();
  auto& tp = test_helper->make_tx_pool(test_app);

  const uint64_t tx_count = 10000;
  for (auto i = 0; i < tx_count; i++) {
    test_app->set_gas(test_detail::rand_gas());
    CHECK_NOTHROW(tp.check_tx_sync(test_helper->new_tx()));
  }

  uint tc = 100;
  for (uint i = 0; i < tc; i++) {
    uint64_t max_bytes = 1000;
    uint64_t max_gas = test_detail::rand_gas(1000000, 100000);
    auto txs = tp.reap_max_bytes_max_gas(max_bytes, max_gas);
    uint64_t total_bytes = 0;
    for (auto& tx : txs) {
      total_bytes += tx.size();
    }
    CHECK(total_bytes <= max_bytes);
  }
}

TEST_CASE("tx_pool: Update", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();

  const uint64_t tx_count = 10;
  auto put_tx = [&](class tx_pool& tp, std::vector<tx>& txs) {
    for (auto& tx : txs) {
      CHECK_NOTHROW(tp.check_tx_sync(tx));
    }
  };

  SECTION("Erase committed tx") {
    auto& tp = test_helper->make_tx_pool();
    auto txs = test_helper->new_txs(tx_count);
    put_tx(tp, txs);
    std::vector<consensus::response_deliver_tx> res;
    res.resize(tx_count);
    tp.update(0, txs, res);
    CHECK(tp.empty());
  }

  SECTION("Erase block height expired tx") {
    config config{
      .ttl_num_blocks = 10,
    };
    auto& tp = test_helper->make_tx_pool(config);
    std::vector<consensus::response_deliver_tx> res;
    std::vector<tx> empty_txs;
    for (uint64_t i = 0; i < tx_count; i++) {
      tp.update(i, empty_txs, res);
      auto txs = test_helper->new_txs(1);
      put_tx(tp, txs);
    }

    for (uint64_t i = 0; i < tx_count; i++) {
      tp.update(config.ttl_num_blocks + i, empty_txs, res);
      CHECK(tp.size() == tx_count - i - 1);
    }
    CHECK(tp.empty());
  }

  SECTION("Erase time expired tx") {
    config config{
      .ttl_duration = std::chrono::microseconds(100000LL).count(), // 100 msec
    };
    auto& tp = test_helper->make_tx_pool(config);
    auto txs = test_helper->new_txs(tx_count);
    put_tx(tp, txs);
    std::vector<consensus::response_deliver_tx> res;
    std::vector<consensus::tx> empty_txs;

    usleep(100000LL); // sleep 100 msec
    tp.update(0, empty_txs, res);
    CHECK(tp.empty());
  }
}

TEST_CASE("tx_pool: Nonce override", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  auto test_app = std::make_shared<test_application>();
  config config;
  auto& tp = test_helper->make_tx_pool(config, test_app);

  auto tx1 = test_helper->new_tx();
  CHECK_NOTHROW(tp.check_tx_sync(tx1));

  auto tx2 = test_helper->new_tx();
  test_app->set_nonce(0);
  CHECK_THROWS_AS(tp.check_tx_sync(tx2), fc::override_fail_exception);

  test_app->set_nonce(0);
  test_app->set_gas(config.gas_price_bump);
  CHECK_NOTHROW(tp.check_tx_sync(tx2));
}

TEST_CASE("LRU_cache: Cache basic test", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  uint tx_count = 1000;
  uint cache_size = 1000;

  LRU_cache<tx_hash, consensus::tx> c{cache_size};

  struct test_tx {
    consensus::tx tx;
    tx_hash hash;
  };
  std::vector<test_tx> txs;
  txs.reserve(tx_count);

  for (uint64_t i = 0; i < tx_count; i++) {
    test_tx test_tx;
    test_tx.tx = test_helper->new_tx();
    test_tx.hash = get_tx_hash(test_tx.tx);
    c.put(test_tx.hash, test_tx.tx);
    txs.push_back(test_tx);
  }

  SECTION("put") {
    CHECK(c.size() == tx_count);
    for (auto& tx : txs) {
      CHECK(c.has(tx.hash));
    }

    // new tx, replace the oldest tx in cache
    auto tx = test_helper->new_tx();
    c.put(get_tx_hash(tx), tx); // tx0 is replaced by new one
    CHECK(c.size() == tx_count);
    CHECK(c.has(get_tx_hash(tx)));
    CHECK(!c.has(txs[0].hash));

    // put again tx1
    c.put(txs[1].hash, txs[1].tx);
    tx = test_helper->new_tx();
    c.put(get_tx_hash(tx), tx); // tx2 is replaced by new one
    CHECK(c.has(get_tx_hash(tx)));
    CHECK(c.has(txs[1].hash));
    CHECK(!c.has(txs[2].hash));
  }

  SECTION("invalid") {
    auto wrapped_tx = test_helper->make_random_wrapped_tx("user");
    auto item = c.get(wrapped_tx->hash());
    CHECK(!item.has_value());
  }

  SECTION("del") {
    CHECK(c.has(txs[3].hash));
    c.del(txs[3].hash);
    CHECK(!c.has(txs[3].hash));
    CHECK(c.size() == tx_count - 1);
  }

  SECTION("get") {
    for (auto& tx : txs) {
      auto res = c.get(tx.hash);
      CHECKED_IF(res.has_value()) {
        CHECK(to_string(tx.hash) == to_string(get_tx_hash(res.value())));
      }
    }
  }
}
