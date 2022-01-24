// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <fc/reflect/variant.hpp>
#include <fc/variant.hpp>

namespace noir::jsonrpc {

enum error_code {
  undefined = 0,
  server_error = -32000,
  parse_error = -32700,
  invalid_request = -32600,
  method_not_found = -32601,
  invalid_params = -32602,
  internal_error = -32603,
};

typedef std::function<fc::variant(const fc::variant&)> request_handler;

struct error {
  error() : code(error_code::undefined) {}

  error(error_code c, std::string m, std::optional<fc::variant> d = std::optional<fc::variant>())
    : code(c), message(m), data(d) {}

  error_code code;
  std::string message;
  std::optional<fc::variant> data;
};

struct response {
  std::string jsonrpc = "2.0";
  std::optional<fc::variant> result;
  std::optional<error> error;
  fc::variant id;

  operator fc::variant() {
    fc::variant out;
    fc::to_variant(*this, out);
    return out;
  }
};

namespace detail {
  class endpoint_impl {
  public:
    void rpc_id(const fc::variant_object&, response&);
    void rpc_jsonrpc(const fc::variant_object& request, response& response);
    response rpc(const fc::variant& message);

    void add_handler(const std::string& method_name, request_handler handler);
    fc::variant handle_request(const std::string& message);

  private:
    std::map<std::string, request_handler> handlers;
  };
} // namespace detail

class endpoint {
public:
  endpoint() : my(new detail::endpoint_impl()) {}

  void add_handler(const std::string& method, request_handler handler);
  fc::variant handle_request(const std::string& message);

private:
  std::unique_ptr<detail::endpoint_impl> my;
};

} // namespace noir::jsonrpc

namespace fc {

inline void from_variant(const variant& v, noir::jsonrpc::error_code& c) {
  c = static_cast<noir::jsonrpc::error_code>(v.as_int64());
}
inline void to_variant(const noir::jsonrpc::error_code& c, variant& v) {
  v = variant(static_cast<int64_t>(c));
}

} // namespace fc

FC_REFLECT(noir::jsonrpc::error, (code)(message)(data))
FC_REFLECT(noir::jsonrpc::response, (jsonrpc)(result)(error)(id))
