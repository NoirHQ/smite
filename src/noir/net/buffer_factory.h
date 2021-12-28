#pragma once
#include <noir/net/protocol.h>
#include <noir/codec/scale.h>

namespace noir::net {

using namespace noir::codec;

using send_buffer_type = std::shared_ptr<std::vector<char>>;

struct buffer_factory {

  /// caches result for subsequent calls, only provide same net_message instance for each invocation
  const send_buffer_type &get_send_buffer(const net_message &m) {
    if (!send_buffer) {
      send_buffer = create_send_buffer(m);
    }
    return send_buffer;
  }

 protected:
  send_buffer_type send_buffer;

 protected:
  static send_buffer_type create_send_buffer(const net_message &m) {
//    auto data = encode<scale>(m);

    const uint32_t payload_size = fc::raw::pack_size(m);

    const char *const header = reinterpret_cast<const char *const>(&payload_size);
    constexpr size_t header_size = sizeof(payload_size);
    static_assert(header_size == message_header_size, "invalid message_header_size");
    const size_t buffer_size = header_size + payload_size;

    auto send_buffer = std::make_shared<std::vector<char>>(buffer_size);
    fc::datastream<char *> ds(send_buffer->data(), buffer_size);
    ds.write(header, header_size);
    fc::raw::pack(ds, m);

    return send_buffer;
  }

//  template<typename T>
//  static send_buffer_type create_send_buffer(uint32_t which, const T &v) {
//    // match net_message static_variant pack
//    const uint32_t which_size = fc::raw::pack_size(unsigned_int(which));
//    const uint32_t payload_size = which_size + fc::raw::pack_size(v);
//
//    const char
//        *const header = reinterpret_cast<const char *const>(&payload_size); // avoid variable size encoding of uint32_t
//    constexpr size_t header_size = sizeof(payload_size);
//    static_assert(header_size == message_header_size, "invalid message_header_size");
//    const size_t buffer_size = header_size + payload_size;
//
//    auto send_buffer = std::make_shared<vector<char>>(buffer_size);
//    fc::datastream<char *> ds(send_buffer->data(), buffer_size);
//    ds.write(header, header_size);
//    fc::raw::pack(ds, unsigned_int(which));
//    fc::raw::pack(ds, v);
//
//    return send_buffer;
//  }

};

}
