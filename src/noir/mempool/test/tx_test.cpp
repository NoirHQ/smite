// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <noir/mempool/tx.h>
#include <range/v3/view/enumerate.hpp>
#include <random>

using namespace noir;
using namespace noir::mempool;

TEST_CASE("TxStore", "[noir][mempool]") {
  SECTION("GetTxBySender") {
    auto txs = TxStore();
    auto wtx = std::make_shared<WrappedTx>(WrappedTx{
      .tx = Bytes(std::span("test_tx")),
      .priority = 1,
      .sender = "foo",
      .timestamp = std::chrono::system_clock::now().time_since_epoch().count(),
    });

    auto res = txs.get_tx_by_sender(wtx->sender);
    CHECK(!res);

    txs.set_tx(wtx);

    res = txs.get_tx_by_sender(wtx->sender);
    CHECK(res);
    CHECK(res == wtx);
  }

  SECTION("GetTxByHash") {
    auto txs = TxStore();
    auto wtx = std::make_shared<WrappedTx>(WrappedTx{
      .tx = Bytes(std::span("test_tx")),
      .priority = 1,
      .sender = "foo",
      .timestamp = std::chrono::system_clock::now().time_since_epoch().count(),
    });

    auto key = wtx->tx.key();
    auto res = txs.get_tx_by_hash(key);
    CHECK(!res);

    txs.set_tx(wtx);

    res = txs.get_tx_by_hash(key);
    CHECK(res);
    CHECK(res == wtx);
  }

  SECTION("SetTx") {
    auto txs = TxStore();
    auto wtx = std::make_shared<WrappedTx>(WrappedTx{
      .tx = Bytes(std::span("test_tx")),
      .priority = 1,
      .timestamp = std::chrono::system_clock::now().time_since_epoch().count(),
    });

    auto key = wtx->tx.key();
    txs.set_tx(wtx);

    auto res = txs.get_tx_by_hash(key);
    CHECK(res);
    CHECK(res == wtx);

    wtx->sender = "foo";
    txs.set_tx(wtx);

    res = txs.get_tx_by_hash(key);
    CHECK(res);
    CHECK(res == wtx);
  }

  SECTION("GetOrSetPeerByTxHash") {
    auto txs = TxStore();
    auto wtx = std::make_shared<WrappedTx>(WrappedTx{
      .tx = Bytes(std::span("test_tx")),
      .priority = 1,
      .timestamp = std::chrono::system_clock::now().time_since_epoch().count(),
    });

    auto key = wtx->tx.key();
    txs.set_tx(wtx);

    auto [res, ok] = txs.get_or_set_peer_by_tx_hash(types::Tx{Bytes(std::span("test_tx_2"))}.key(), 15);
    CHECK(!res);
    CHECK(!ok);

    std::tie(res, ok) = txs.get_or_set_peer_by_tx_hash(key, 15);
    CHECK(res);
    CHECK(!ok);

    std::tie(res, ok) = txs.get_or_set_peer_by_tx_hash(key, 15);
    CHECK(res);
    CHECK(ok);

    CHECK(txs.tx_has_peer(key, 15));
    CHECK(txs.tx_has_peer(key, 16) == false);
  }

  SECTION("RemoveTx") {
    auto txs = TxStore();
    auto wtx = std::make_shared<WrappedTx>(WrappedTx{
      .tx = Bytes(std::span("test_tx")),
      .priority = 1,
      .timestamp = std::chrono::system_clock::now().time_since_epoch().count(),
    });

    txs.set_tx(wtx);

    auto key = wtx->tx.key();
    auto res = txs.get_tx_by_hash(key);
    CHECK(res);

    txs.remove_tx(res);

    res = txs.get_tx_by_hash(key);
    CHECK(!res);
  }

  SECTION("Size") {
    auto tx_store = TxStore();
    auto num_txs = 1000;

    for (auto i = 0; i < num_txs; i++) {
      auto tx = fmt::format("test_tx_{:d}", i);
      tx_store.set_tx(std::make_shared<WrappedTx>(WrappedTx{
        .tx = Bytes(std::span(tx)),
        .priority = i,
        .timestamp = std::chrono::system_clock::now().time_since_epoch().count(),
      }));
    }

    CHECK(num_txs == tx_store.size());
  }
}

TEST_CASE("WrappedTxList", "[noir][mempool]") {
  SECTION("Reset") {
    auto list = WrappedTxList([](auto wtx1, auto wtx2) -> bool { return wtx1->height >= wtx2->height; });

    CHECK(0 == list.size());

    for (auto i = 0; i < 100; i++) {
      auto wtx = std::make_shared<WrappedTx>(WrappedTx{.height = i});
      list.insert(std::move(wtx));
    }

    CHECK(100 == list.size());

    list.reset();
    CHECK(0 == list.size());
  }

  SECTION("Insert") {
    auto list = WrappedTxList([](auto wtx1, auto wtx2) -> bool { return wtx1->height >= wtx2->height; });

    auto gen = std::mt19937((std::chrono::steady_clock::now().time_since_epoch().count()));
    auto rng = std::uniform_int_distribution<>(0, 9999);

    auto expected = std::vector<int64_t>();
    for (auto i = 0; i < 100; i++) {
      auto height = rng(gen);
      expected.push_back(height);

      auto wtx = std::make_shared<WrappedTx>(WrappedTx{.height = height});
      list.insert(std::move(wtx));

      if (i % 10 == 0) {
        auto wtx = std::make_shared<WrappedTx>(WrappedTx{.height = height});
        list.insert(std::move(wtx));

        expected.push_back(height);
      }
    }

    auto got = std::vector<int64_t>(list.size());
    for (auto [i, wtx] : ranges::views::enumerate(list.txs)) {
      got[i] = wtx->height;
    }

    std::sort(expected.begin(), expected.end());
    CHECK(expected == got);
  }

  SECTION("Remove") {
    auto list = WrappedTxList([](auto wtx1, auto wtx2) -> bool { return wtx1->height >= wtx2->height; });

    auto gen = std::mt19937((std::chrono::steady_clock::now().time_since_epoch().count()));
    auto rng = std::uniform_int_distribution<>(0, 9999);

    std::vector<std::shared_ptr<WrappedTx>> txs;
    for (auto i = 0; i < 100; i++) {
      auto height = rng(gen);
      auto tx = std::make_shared<WrappedTx>(WrappedTx{.height = height});

      txs.push_back(tx);
      list.insert(std::move(tx));

      if (i % 10 == 0) {
        auto tx = std::make_shared<WrappedTx>(WrappedTx{.height = height});
        list.insert(tx);
        txs.push_back(std::move(tx));
      }
    }

    // remove a tx that does not exist
    list.remove(std::make_shared<WrappedTx>(WrappedTx{.height = 20000}));

    // remove a tx that exists (by height) but not referenced
    list.remove(std::make_shared<WrappedTx>(WrappedTx{.height = txs[0]->height}));

    // remove a few existing txs
    for (auto i = 0; i < 25; i++) {
      auto j = std::uniform_int_distribution<>(0, txs.size() - 1)(gen);
      list.remove(txs[j]);
      txs.erase(txs.begin() + j);
    }

    auto expected = std::vector<int64_t>(txs.size());
    for (auto [i, tx] : ranges::views::enumerate(txs)) {
      expected[i] = tx->height;
    }

    auto got = std::vector<int64_t>(list.size());
    for (auto [i, wtx] : ranges::views::enumerate(list.txs)) {
      got[i] = wtx->height;
    }

    std::sort(expected.begin(), expected.end());
    CHECK(expected == got);
  }
}
