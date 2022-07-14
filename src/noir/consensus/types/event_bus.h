// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/consensus/types/events.h>
#include <appbase/application.hpp>
#include <appbase/channel.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace noir::consensus::events {

struct message {
  std::string sub_id;
  tm_event_data data;
  std::vector<::tendermint::abci::Event> events;
};

class event_bus {
public:
  using tm_pub_sub = appbase::channel_decl<struct event_bus_tag, message>;

  event_bus(appbase::application& app)
    : event_bus_channel_(app.get_channel<tm_pub_sub>()),
      subscription_map_(std::make_shared<subscription_map_type_>()) {}

  bool has_subscribers() const {
    return event_bus_channel_.has_subscribers();
  }

  struct subscription {
    std::string subscriber;
    std::string id;

    ~subscription() {
      unsubscribe();
    }

    // can be constructed and moved
    subscription() = default;
    subscription(subscription&&) = default;
    subscription& operator=(subscription&&) = default;

    // cannot copy
    subscription(const subscription&) = delete;
    subscription& operator=(const subscription&) = delete;

    std::optional<std::string> unsubscribe() {
      auto handle_ptr = handle_.lock();
      auto map_ptr = map_.lock();
      if (!handle_ptr || !map_ptr) {
        return "invalid subscription";
      }
      handle_ptr->unsubscribe();

      auto it = map_ptr->find(subscriber);
      if (it == map_ptr->end()) {
        return fmt::format("invalid subscriber:{}", subscriber);
      }
      auto it2 = it->second.find(id);
      if (it2 == it->second.end()) {
        return fmt::format("invalid subscription id: {}", id);
      }
      it->second.erase(id);
      if (it->second.size() == 0) { // if all elements are deleted
        map_ptr->erase(subscriber);
      }
      return {};
    }

  private:
    friend class event_bus;
    using handle_type_ = tm_pub_sub::channel_type::handle;
    using subscription_map_type_ =
      std::map<std::string, std::map<std::string, std::shared_ptr<tm_pub_sub::channel_type::handle>>>;
    std::weak_ptr<handle_type_> handle_;
    std::weak_ptr<subscription_map_type_> map_;

    subscription(std::string subscriber,
      std::string id,
      std::shared_ptr<tm_pub_sub::channel_type::handle> handle_,
      std::shared_ptr<subscription_map_type_> map_)
      : subscriber(subscriber), id(id), handle_(handle_), map_(map_) {}
  };

  size_t num_clients() const {
    return subscription_map_->size();
  }

  size_t num_client_subscription(const std::string& subscriber) {
    auto it = subscription_map_->find(subscriber);
    return (it == subscription_map_->end()) ? 0 : it->second.size();
  }

  template<typename Callback>
  [[nodiscard]] subscription subscribe(const std::string& subscriber, Callback cb) {
    auto handle_ = std::make_shared<tm_pub_sub::channel_type::handle>(event_bus_channel_.subscribe(cb));
    subscription ret(
      subscriber, boost::uuids::to_string(boost::uuids::random_generator()()), handle_, subscription_map_);
    (*subscription_map_)[subscriber][ret.id] = std::move(handle_); // FIXME: handle duplicated case
    return std::move(ret);
  }

  std::optional<std::string> unsubscribe(subscription& handle) {
    return handle.unsubscribe();
  }

  std::optional<std::string> unsubscribe_all(const std::string& subscriber) {
    auto it = subscription_map_->find(subscriber);
    if (it == subscription_map_->end()) {
      return fmt::format("invalid subscriber:{}", subscriber);
    }
    for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
      it2->second->unsubscribe();
    }
    subscription_map_->erase(subscriber);
    return {};
  }

  void publish(const std::string& event_value, const tm_event_data& data) {
    ::tendermint::abci::Event ev;
    ev.set_type("tm");
    auto attrs = ev.mutable_attributes();
    auto attr = attrs->Add();
    attr->set_key("event");
    attr->set_value(event_value);
    publish_with_events(data, {{ev}});
  }

  void publish_event_new_block(const event_data_new_block& data) {
    std::vector<::tendermint::abci::Event> events;
    events.insert(events.end(), data.result_begin_block.events().begin(), data.result_begin_block.events().end());
    events.insert(events.end(), data.result_end_block.events().begin(), data.result_end_block.events().end());
    // add Tendermint-reserved new block event
    events.push_back(prepopulated_event<event_value::new_block>());
    publish_with_events(data, events);
  }

  void publish_event_new_block_header(const event_data_new_block_header& data) {
    std::vector<::tendermint::abci::Event> events;
    events.insert(events.end(), data.result_begin_block.events().begin(), data.result_begin_block.events().end());
    events.insert(events.end(), data.result_end_block.events().begin(), data.result_end_block.events().end());
    // add Tendermint-reserved new block event
    events.push_back(prepopulated_event<event_value::new_block_header>());
    publish_with_events(data, events);
  }

  void publish_event_new_evidence(const event_data_new_evidence& data) {
    publish(string_from_event_value(event_value::new_evidence), data);
  }

  void publish_event_vote(const event_data_vote& data) {
    publish(string_from_event_value(event_value::vote), data);
  }

  void publish_event_valid_block(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::valid_block), data);
  }

  void publish_event_block_sync_status(const event_data_block_sync_status& data) {
    publish(string_from_event_value(event_value::block_sync_status), data);
  }

  void publish_event_state_sync_status(const event_data_state_sync_status& data) {
    publish(string_from_event_value(event_value::state_sync_status), data);
  }

  /// publishes tx event with events from Result. Note it will add
  /// predefined keys (EventTypeKey, TxHashKey). Existing events with the same keys
  /// will be overwritten.
  /// \param[in] data
  void publish_event_tx(const event_data_tx& data) {
    std::vector<::tendermint::abci::Event> events;
    events.insert(events.end(), data.tx_result.result().events().begin(), data.tx_result.result().events().end());

    // add Tendermint-reserved events
    events.push_back(prepopulated_event<event_value::tx>());

    ::tendermint::abci::Event ev_hash;
    ev_hash.set_type("tx");
    auto attr_hash = ev_hash.mutable_attributes()->Add();
    attr_hash->set_key("hash");
    attr_hash->set_value(get_tx_hash(data.tx_result.tx()).to_string());
    events.push_back({ev_hash});

    ::tendermint::abci::Event ev_height;
    ev_height.set_type("tx");
    auto attr_height = ev_height.mutable_attributes()->Add();
    attr_height->set_key("height");
    attr_height->set_value(std::to_string(data.tx_result.height()));
    events.push_back({ev_height});

    publish_with_events(data, events);
  }

  void publish_event_new_round_step(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::new_round_step), data);
  }
  void publish_event_timeout_propose(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::timeout_propose), data);
  }
  void publish_event_timeout_wait(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::timeout_wait), data);
  }
  void publish_event_new_round(const event_data_new_round& data) {
    publish(string_from_event_value(event_value::new_round), data);
  }
  void publish_event_complete_proposal(const event_data_complete_proposal& data) {
    publish(string_from_event_value(event_value::complete_proposal), data);
  }
  void publish_event_polka(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::polka), data);
  }
  void publish_event_unlock(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::unlock), data);
  }
  void publish_event_relock(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::relock), data);
  }
  void publish_event_lock(const event_data_round_state& data) {
    publish(string_from_event_value(event_value::lock), data);
  }
  void publish_event_validator_set_updates(const event_data_validator_set_updates& data) {
    publish(string_from_event_value(event_value::validator_set_updates), data);
  }

private:
  using subscription_map_type_ =
    std::map<std::string, std::map<std::string, std::shared_ptr<tm_pub_sub::channel_type::handle>>>;
  static constexpr int priority_ = appbase::priority::medium;
  tm_pub_sub::channel_type& event_bus_channel_;
  std::shared_ptr<subscription_map_type_> subscription_map_;

  inline void publish_with_events(const tm_event_data& data, const std::vector<::tendermint::abci::Event>& events) {
    message msg{
      // .sub_id,
      .data = data,
      .events = events,
    };
    event_bus_channel_.publish(priority_, msg);
  }
};

} // namespace noir::consensus::events
