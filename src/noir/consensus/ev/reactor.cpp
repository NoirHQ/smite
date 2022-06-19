// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/core/core.h>

#include <noir/common/check.h>
#include <noir/common/scope_exit.h>
#include <noir/consensus/ev/reactor.h>

namespace noir::consensus::ev {

constexpr int broadcast_evidence_interval_s = 10;

void reactor::process_peer_update(plugin_interface::peer_status_info_ptr info) {
  dlog(fmt::format(
    "[es_reactor] peer update: peer_id={}, status={}", info->peer_id, p2p::peer_status_to_str(info->status)));
  // std::scoped_lock _(mtx); // TODO : really needed mutex?
  switch (info->status) {
  case p2p::peer_status::up: {
    if (!peer_routines.contains(info->peer_id)) {
      auto it = peer_routines.insert(std::make_pair(info->peer_id, Chan<std::monostate>{thread_pool->get_executor()}));
      peer_wg.add(1);
      broadcast_evidence_loop(info->peer_id, it.first->second);
    }
    break;
  }
  case p2p::peer_status::down: {
    if (auto it = peer_routines.find(info->peer_id); it != peer_routines.end()) {
      it->second.close();
    }
    break;
  }
  default:
    break;
  }
}

Result<void> reactor::process_peer_msg(p2p::envelope_ptr info) {
  auto from = info->from;
  auto to = info->broadcast ? "all" : info->to;

  // datastream<char> ds(info->message.data(), info->message.size());
  ::tendermint::types::EvidenceList
    msg; // TODO : convert from envelope to pb; requires changing all existing envelopes serialization

  dlog(fmt::format("[es_reactor] recv msg. from={}, to={}", from, to));

  for (const auto& ev : msg.evidence()) {
    if (auto ok = evidence::from_proto(ev); !ok) {
      elog(fmt::format("failed to convert evidence: {}", ok.error().message()));
      continue;
    } else {
      if (auto r = evpool->add_evidence(ok.value()); !r) {
        return r.error(); // TODO : check; requires removing peer
      }
    }
  }
  return success();
}

void reactor::broadcast_evidence_loop(const std::string& peer_id, Chan<std::monostate>& closer) {
  boost::asio::co_spawn(thread_pool->get_executor(), [&]() -> boost::asio::awaitable<void> {
    clist::CElementPtr<std::shared_ptr<evidence>> next{};

    auto _ = make_scope_exit([&]() {
      {
        std::unique_lock g{mtx};
        peer_routines.erase(peer_id);

        peer_wg.done();

        // TODO: need check
        // if (auto e = recover(); e) {
        // }
      }
    });

    for (;;) {
      if (!next) {
        auto res = co_await (evpool->evidence_wait_chan().async_receive(eo::eoroutine)
          || closer.async_receive(eo::eoroutine)
          // TODO: implement tendermint::service
          /* close */
        );
        switch (res.index()) {
        case 0:
          if (next = evpool->evidence_front(); !next) {
            continue;
          }
        case 1:
          co_return;
        /*
        case 2:
          co_return;
        */
        }
      }

      auto ev = next->value;
      auto ev_proto = evidence::to_proto(*ev);
      // check(ev_proto, fmt::format("failed to convert evidence: {}", ev_proto.error()));

      auto new_env = std::make_shared<p2p::envelope>();
      new_env->to = peer_id;
      new_env->broadcast = false;
      new_env->id = p2p::Evidence;
      ::tendermint::types::EvidenceList msg;
      *msg.add_evidence() = *ev_proto.value();
      new_env->message.resize(ev_proto.value()->ByteSizeLong());
      ev_proto.value()->SerializeToArray(new_env->message.data(), ev_proto.value()->ByteSizeLong());
      xmt_mq_channel.publish(appbase::priority::medium, new_env);
      dlog(fmt::format("gossiped evidence to peer={}", peer_id));

      auto timer = boost::asio::steady_timer(thread_pool->get_executor(), std::chrono::seconds(broadcast_evidence_interval_s));

      auto res = co_await (timer.async_wait(eo::eoroutine)
        || next->next_wait_chan().async_receive(eo::eoroutine)
        || closer.async_receive(eo::eoroutine)
        // TODO: implement tendermint::service
        // || close_ch
      );
      switch (res.index()) {
      case 0:
        next = nullptr;
        break;
      case 1:
        next = next->next();
        break;
      case 2:
        co_return;
      /*
      case 3:
        co_return;
      */
      }
    }
  }, boost::asio::detached);
}

} // namespace noir::consensus::ev
