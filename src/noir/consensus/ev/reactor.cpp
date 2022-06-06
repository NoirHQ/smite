// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/consensus/ev/reactor.h>

namespace noir::consensus::ev {

void reactor::process_peer_update(plugin_interface::peer_status_info_ptr info) {
  dlog(fmt::format(
    "[es_reactor] peer update: peer_id={}, status={}", info->peer_id, p2p::peer_status_to_str(info->status)));
  // std::scoped_lock _(mtx); // TODO : really needed mutex?
  switch (info->status) {
  case p2p::peer_status::up: {
    auto it = peer_routines.find(info->peer_id);
    if (it == peer_routines.end()) {
      auto closer = std::make_shared<bool>(false);
      peer_routines.insert({info->peer_id, closer});
      broadcast_evidence_loop(info->peer_id, closer);
    }
    break;
  }
  case p2p::peer_status::down: {
    if (auto it = peer_routines.find(info->peer_id); it != peer_routines.end()) {
      *it->second = true;
      peer_routines.erase(it);
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

  for (auto i = 0; i < msg.evidence().size(); i++) {
    if (auto ok = evidence::from_proto(msg.evidence().at(i)); !ok) {
      elog(fmt::format("failed to convert evidence: {}", ok.error().message()));
      continue;
    } else {
      if (auto r = pool->add_evidence(ok.value()); !r) {
        return r.error(); // TODO : check; requires removing peer
      }
    }
  }
  return success();
}

void reactor::broadcast_evidence_loop(const std::string& peer_id, const std::shared_ptr<bool>& closer) {
  thread_pool->get_executor().post([&]() {
    std::shared_ptr<evidence> next{};

    while (true) {
      if (*closer)
        return;
      if (!next) {
        if (next = pool->evidence_front(); !next)
          continue;
      }

      auto ev = next;
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

      // TODO : next = next->next; which requires to use thread safe list
    }
  });
}

} // namespace noir::consensus::ev
