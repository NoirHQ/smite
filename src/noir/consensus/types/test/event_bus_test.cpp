// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/overloaded.h>
#include <noir/common/thread_pool.h>
#include <noir/consensus/common_test.h>
#include <noir/consensus/types/event_bus.h>
#include <noir/consensus/types/events.h>
#include <appbase/application.hpp>
#include <fmt/core.h>

namespace {
using namespace noir::consensus;
using namespace noir::consensus::events;
using namespace std::chrono_literals;

TEST_CASE("event_bus: subscribe/unsubscribe", "[noir][consensus][events]") {
  appbase::application app_;
  app_.register_plugin<test_plugin>();
  app_.initialize<test_plugin>();
  event_bus ev_bus(app_);

  SECTION("basic") {
    CHECK(ev_bus.has_subscribers() == false);
    CHECK(ev_bus.num_clients() == 0);

    event_bus::subscription invalid_subscription{};
    invalid_subscription.subscriber = "invalid_subscriber";
    invalid_subscription.id = "invalid_id";

    CHECK(ev_bus.unsubscribe(invalid_subscription) != std::nullopt);
    CHECK(ev_bus.unsubscribe_all("invalid_id") != std::nullopt);

    {
      auto handle = ev_bus.subscribe("test", [&](const message msg) {});
      CHECK(ev_bus.has_subscribers() == true);
      CHECK(ev_bus.num_clients() == 1);
      CHECK(ev_bus.num_client_subscription("test") == 1);
    }
    // unsubscribed due to RAII
    CHECK(ev_bus.has_subscribers() == false);
    CHECK(ev_bus.num_clients() == 0);
    CHECK(ev_bus.num_client_subscription("test") == 0);

    // destroying unsubscribed handle should not panic
    CHECK_NOTHROW([&]() {
      auto handle = ev_bus.subscribe("test", [&](const message msg) {});
      handle.unsubscribe();
    }());
  }

  SECTION("subscribe with 1 subscriber") {
    std::array<event_bus::subscription, 10> handles;
    for (size_t i = 0; i < 10; ++i) {
      handles[i] = ev_bus.subscribe("test", [&](const message msg) {});
      CHECK(ev_bus.has_subscribers() == true);
      CHECK(ev_bus.num_clients() == 1);
      CHECK(ev_bus.num_client_subscription("test") == i + 1);
    }
    SECTION("unsubscribe") {
      for (size_t i = 9; i > 0; --i) {
        CHECK(ev_bus.unsubscribe(handles[i]) == std::nullopt);
        CHECK(ev_bus.has_subscribers() == true);
        CHECK(ev_bus.num_clients() == 1);
        CHECK(ev_bus.num_client_subscription("test") == i);
      }
      // unsubscribe last element
      CHECK(ev_bus.unsubscribe(handles[0]) == std::nullopt);
      CHECK(ev_bus.has_subscribers() == false);
      CHECK(ev_bus.num_clients() == 0);
      CHECK(ev_bus.num_client_subscription("test") == 0);
    }
    SECTION("unsubscribe_all") {
      CHECK(ev_bus.unsubscribe_all("test") == std::nullopt);
      CHECK(ev_bus.has_subscribers() == false);
      CHECK(ev_bus.num_clients() == 0);
      CHECK(ev_bus.num_client_subscription("test") == 0);
    }
  }

  SECTION("subscribe with multiple subscriber") {
    std::array<event_bus::subscription, 10> handles;
    for (size_t i = 0; i < 10; ++i) {
      auto subscriber = "test" + std::to_string(i);
      handles[i] = ev_bus.subscribe(subscriber, [&](const message msg) {});
      CHECK(ev_bus.has_subscribers() == true);
      CHECK(ev_bus.num_clients() == i + 1);
      CHECK(ev_bus.num_client_subscription(subscriber) == 1);
    }
    SECTION("unsubscribe") {
      for (size_t i = 9; i > 0; --i) {
        auto subscriber = "test" + std::to_string(i);
        CHECK(ev_bus.unsubscribe(handles[i]) == std::nullopt);
        CHECK(ev_bus.has_subscribers() == true);
        CHECK(ev_bus.num_clients() == i);
        CHECK(ev_bus.num_client_subscription(subscriber) == 0);
      }
      // unsubscribe last element
      CHECK(ev_bus.unsubscribe(handles[0]) == std::nullopt);
      CHECK(ev_bus.has_subscribers() == false);
      CHECK(ev_bus.num_clients() == 0);
      CHECK(ev_bus.num_client_subscription("test0") == 0);
    }
    SECTION("unsubscribe_all") {
      for (size_t i = 9; i > 0; --i) {
        auto subscriber = "test" + std::to_string(i);
        CHECK(ev_bus.unsubscribe_all(subscriber) == std::nullopt);
        CHECK(ev_bus.has_subscribers() == true);
        CHECK(ev_bus.num_clients() == i);
        CHECK(ev_bus.num_client_subscription(subscriber) == 0);
      }
      // unsubscribe last subscriber
      CHECK(ev_bus.unsubscribe_all("test0") == std::nullopt);
      CHECK(ev_bus.has_subscribers() == false);
      CHECK(ev_bus.num_clients() == 0);
      CHECK(ev_bus.num_client_subscription("test0") == 0);
    }
  }
}

TEST_CASE("event_bus: pub/sub", "[noir][consensus][events]") {
  struct test_case {
    std::string name;
    std::optional<tm_event_data> msg;
    bool expect_receive;
    bool expect_pass;
  };
  auto tests = std::to_array<test_case>({
    {"pub/sub no pub", std::nullopt, false, false},
    {"pub/sub data_type: new_block", event_data_new_block{}, true, true},
    {"pub/sub data_type: new_block_header", event_data_new_block_header{}, true, true},
    {"pub/sub data_type: new_evidence", event_data_new_evidence{}, true, true},
    {"pub/sub data_type: tx", event_data_tx{}, true, true},
    {"pub/sub data_type: new_round", event_data_new_round{}, true, true},
    {"pub/sub data_type: complete_proposal", event_data_complete_proposal{}, true, true},
    {"pub/sub data_type: vote", event_data_vote{}, true, true},
    {"pub/sub data_type: string", event_data_string{"event_data_string"}, true, true},
    {"pub/sub data_type: validator_set_updates", event_data_validator_set_updates{}, true, true},
    {"pub/sub data_type: block_sync_status", event_data_block_sync_status{}, true, true},
    {"pub/sub data_type: state_sync_status", event_data_state_sync_status{}, true, true},
    {"pub/sub data_type: complete_proposal", event_data_complete_proposal{}, true, true},
  });

  std::for_each(tests.begin(), tests.end(), [](const test_case& t) {
    SECTION(t.name) {
      appbase::application app_;

      bool received = false;
      bool passed = false;

      uint64_t max_thread_num = 10;
      uint64_t thread_num = std::min<uint64_t>(5, max_thread_num);
      auto thread = std::make_unique<noir::named_thread_pool>("test_thread", thread_num);
      auto res = noir::async_thread_pool(thread->get_executor(), [&]() {
        app_.register_plugin<test_plugin>();
        app_.initialize<test_plugin>();
        app_.startup();
        event_bus ev_bus(app_);
        auto handle = ev_bus.subscribe("test", [&](const message msg) {
          // TODO: elaborate test pass logic for each msg
          std::visit(noir::overloaded{
                       [&](const event_data_new_block& msg) {
                         auto* exp = std::get_if<event_data_new_block>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_new_block_header& msg) {
                         auto* exp = std::get_if<event_data_new_block_header>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_new_evidence& msg) {
                         auto* exp = std::get_if<event_data_new_evidence>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_tx& msg) {
                         auto* exp = std::get_if<event_data_tx>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_new_round& msg) {
                         auto* exp = std::get_if<event_data_new_round>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_complete_proposal& msg) {
                         auto* exp = std::get_if<event_data_complete_proposal>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_vote& msg) {
                         auto* exp = std::get_if<event_data_vote>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_string& str) {
                         auto* exp = std::get_if<event_data_string>(&*t.msg);
                         passed = (exp && (exp->string == str.string));
                       },
                       [&](const event_data_validator_set_updates& msg) {
                         auto* exp = std::get_if<event_data_validator_set_updates>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_block_sync_status& msg) {
                         auto* exp = std::get_if<event_data_block_sync_status>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const event_data_state_sync_status& msg) {
                         auto* exp = std::get_if<event_data_state_sync_status>(&*t.msg);
                         passed = !!exp;
                       },
                       [&](const auto& msg) {
                         std::cout << "invalid msg type" << std::endl;
                         passed = false;
                       },
                     },
            msg.data);
          received = true;
          app_.quit();
        });

        if (t.msg) {
          ev_bus.publish("test", *t.msg);
        }
        app_.exec();
      });

      auto status = res.wait_for(1s);
      CHECKED_ELSE(status == std::future_status::ready) {
        app_.quit();
      }

      CHECK(received == t.expect_receive);
      CHECK(passed == t.expect_pass);
    }
  });
}

TEST_CASE("event_bus: pub/sub prepopulated events", "[noir][consensus][events]") {
  appbase::application app_;

  bool received = false;
  event_value exp{};
  event_value result{};

  app_.register_plugin<test_plugin>();
  app_.initialize<test_plugin>();
  app_.startup();
  event_bus ev_bus(app_);

  auto handle = ev_bus.subscribe("test", [&](const message msg) {
    // TODO: elaborate test pass logic for each msg
    std::visit(noir::overloaded{
                 [&](const event_data_new_block& msg) { result = event_value::new_block; },
                 [&](const event_data_new_block_header& msg) { result = event_value::new_block_header; },
                 [&](const event_data_new_evidence& msg) { result = event_value::new_evidence; },
                 [&](const event_data_tx& msg) { result = event_value::tx; },
                 [&](const event_data_new_round& msg) { result = event_value::new_round; },
                 [&](const event_data_complete_proposal& msg) { result = event_value::complete_proposal; },
                 [&](const event_data_vote& msg) { result = event_value::vote; },
                 [&](const event_data_validator_set_updates& msg) { result = event_value::validator_set_updates; },
                 [&](const event_data_block_sync_status& msg) { result = event_value::block_sync_status; },
                 [&](const event_data_state_sync_status& msg) { result = event_value::state_sync_status; },
                 [&](const event_data_round_state& data) {
                   for (auto i = static_cast<int>(event_value::new_block); i < static_cast<int>(event_value::unknown);
                        ++i) {
                     auto str = string_from_event_value(static_cast<event_value>(i));
                     // TODO: hard coded test logic
                     if (msg.events[0].attributes(0).value() == str) {
                       result = static_cast<event_value>(i);
                       break;
                     }
                   }
                 },
                 [&](const auto& msg) { result = event_value::unknown; },
               },
      msg.data);
    received = true;
    app_.quit();
  });

  uint64_t max_thread_num = 10;
  uint64_t thread_num = std::min<uint64_t>(5, max_thread_num);
  auto thread = std::make_unique<noir::named_thread_pool>("test_thread", thread_num);
  auto res = noir::async_thread_pool(thread->get_executor(), [&]() { app_.exec(); });

  SECTION("publish_event_new_block") {
    ev_bus.publish_event_new_block(event_data_new_block{});
    exp = event_value::new_block;
  }
  SECTION("publish_event_new_block_header") {
    ev_bus.publish_event_new_block_header(event_data_new_block_header{});
    exp = event_value::new_block_header;
  }
  SECTION("publish_event_new_evidence") {
    ev_bus.publish_event_new_evidence(event_data_new_evidence{});
    exp = event_value::new_evidence;
  }
  SECTION("publish_event_vote") {
    ev_bus.publish_event_vote(event_data_vote{});
    exp = event_value::vote;
  }
  SECTION("publish_event_valid_block") {
    ev_bus.publish_event_valid_block(event_data_round_state{});
    exp = event_value::valid_block;
  }
  SECTION("publish_event_block_sync_status") {
    ev_bus.publish_event_block_sync_status(event_data_block_sync_status{});
    exp = event_value::block_sync_status;
  }
  SECTION("publish_event_tx") {
    ev_bus.publish_event_tx(event_data_tx{});
    exp = event_value::tx;
  }
  SECTION("publish_event_new_round_step") {
    ev_bus.publish_event_new_round_step(event_data_round_state{});
    exp = event_value::new_round_step;
  }
  SECTION("publish_event_timeout_propose") {
    ev_bus.publish_event_timeout_propose(event_data_round_state{});
    exp = event_value::timeout_propose;
  }
  SECTION("publish_event_timeout_wait") {
    ev_bus.publish_event_timeout_wait(event_data_round_state{});
    exp = event_value::timeout_wait;
  }
  SECTION("publish_event_new_round") {
    ev_bus.publish_event_new_round(event_data_new_round{});
    exp = event_value::new_round;
  }
  SECTION("publish_event_complete_proposal") {
    ev_bus.publish_event_complete_proposal(event_data_complete_proposal{});
    exp = event_value::complete_proposal;
  }
  SECTION("publish_event_polka") {
    ev_bus.publish_event_polka(event_data_round_state{});
    exp = event_value::polka;
  }
  SECTION("publish_event_unlock") {
    ev_bus.publish_event_unlock(event_data_round_state{});
    exp = event_value::unlock;
  }
  SECTION("publish_event_relock") {
    ev_bus.publish_event_relock(event_data_round_state{});
    exp = event_value::relock;
  }
  SECTION("publish_event_lock") {
    ev_bus.publish_event_lock(event_data_round_state{});
    exp = event_value::lock;
  }
  SECTION("publish_event_validator_set_updates") {
    ev_bus.publish_event_validator_set_updates(event_data_validator_set_updates{});
    exp = event_value::validator_set_updates;
  }

  auto status = res.wait_for(1s);
  CHECKED_ELSE(status == std::future_status::ready) {
    app_.quit();
  }

  CHECK(received == true);
  CHECK(exp == result);
}

} // namespace
