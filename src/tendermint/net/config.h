#pragma once

namespace tendermint::net {

constexpr auto def_send_buffer_size_mb = 4;
constexpr auto def_send_buffer_size = 1024 * 1024 * def_send_buffer_size_mb;
constexpr auto def_max_write_queue_size = def_send_buffer_size * 10;

} // namespace tendermint::net
