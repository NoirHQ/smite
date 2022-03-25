// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#include <noir/rpc/rpc.h>
//#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
//#include <noir/rpc/local_endpoint.h>
//#endif
#include <noir/common/thread_pool.h>
#include <noir/rpc/websocket/websocket.h>

#include <boost/asio.hpp>
#include <boost/optional.hpp>

#include <fc/crypto/openssl.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/network/ip.hpp>
#include <fc/reflect/variant.hpp>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/debug_asio_no_tls.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/logger/stub.hpp>
#include <websocketpp/server.hpp>

#include <memory>
#include <regex>
#include <thread>

const fc::string logger_name("rpc");
fc::logger logger;

namespace noir::rpc {

namespace asio = boost::asio;

using namespace appbase;

using boost::optional;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;
using boost::asio::ip::tcp;
using std::map;
using std::regex;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;
using websocketpp::connection_hdl;

enum https_ecdh_curve_t {
  SECP384R1,
  PRIME256V1
};
std::map<string, https_ecdh_curve_t> https_ecdh_curve_map{
  {"secp384r1", https_ecdh_curve_t::SECP384R1}, {"prime256v1", https_ecdh_curve_t::PRIME256V1}};

static rpc_defaults current_rpc_defaults;

void rpc::set_defaults(const rpc_defaults& config) {
  current_rpc_defaults = config;
}

namespace detail {

  template<class T>
  struct asio_with_stub_log : public websocketpp::config::asio {
    typedef asio_with_stub_log type;
    typedef asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef websocketpp::log::stub elog_type;
    typedef websocketpp::log::stub alog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
      typedef type::concurrency_type concurrency_type;
      typedef type::alog_type alog_type;
      typedef type::elog_type elog_type;
      typedef type::request_type request_type;
      typedef type::response_type response_type;
      typedef T socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

    static const long timeout_open_handshake = 0;
  };

  //#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
  // struct asio_local_with_stub_log : public websocketpp::config::asio {
  //  typedef asio_local_with_stub_log type;
  //  typedef asio base;
  //
  //  typedef base::concurrency_type concurrency_type;
  //
  //  typedef base::request_type request_type;
  //  typedef base::response_type response_type;
  //
  //  typedef base::message_type message_type;
  //  typedef base::con_msg_manager_type con_msg_manager_type;
  //  typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;
  //
  //  typedef websocketpp::log::stub elog_type;
  //  typedef websocketpp::log::stub alog_type;
  //
  //  typedef base::rng_type rng_type;
  //
  //  struct transport_config : public base::transport_config {
  //    typedef type::concurrency_type concurrency_type;
  //    typedef type::alog_type alog_type;
  //    typedef type::elog_type elog_type;
  //    typedef type::request_type request_type;
  //    typedef type::response_type response_type;
  //    typedef websocketpp::transport::asio::basic_socket::local_endpoint socket_type;
  //  };
  //
  //  typedef websocketpp::transport::asio::local_endpoint<transport_config> transport_type;
  //
  //  static const long timeout_open_handshake = 0;
  //};
  //#endif

  /**
   * virtualized wrapper for the various underlying connection functions needed in req/resp processing
   */
  struct abstract_conn {
    virtual ~abstract_conn() = default;
    virtual bool verify_max_bytes_in_flight() = 0;
    virtual bool verify_max_requests_in_flight() = 0;
    virtual void handle_exception() = 0;

    virtual void send_response(std::optional<std::string> body, int code) = 0;
  };

  using abstract_conn_ptr = std::shared_ptr<abstract_conn>;

  template<typename T>
  using connection_ptr = typename websocketpp::server<T>::connection_ptr;

  /**
   * internal url handler that contains more parameters than the handlers provided by external systems
   */
  using internal_url_handler = std::function<void(abstract_conn_ptr, string, string, url_response_callback)>;

  /**
   * Helper method to calculate the "in flight" size of a string
   * @param s - the string
   * @return in flight size of s
   */
  static size_t in_flight_sizeof(const string& s) {
    return s.size();
  }

  /**
   * Helper method to calculate the "in flight" size of a fc::variant
   * This is an estimate based on fc::raw::pack if that process can be successfully executed
   *
   * @param v - the fc::variant
   * @return in flight size of v
   */
  static size_t in_flight_sizeof(const fc::variant& v) {
    try {
      return fc::raw::pack_size(v);
    } catch (...) {
    }
    return 0;
  }

  /**
   * Helper method to calculate the "in flight" size of a std::optional<T>
   * When the optional doesn't contain value, it will return the size of 0
   *
   * @param o - the std::optional<T> where T is typename
   * @return in flight size of o
   */
  template<typename T>
  static size_t in_flight_sizeof(const std::optional<T>& o) {
    if (o) {
      return in_flight_sizeof(*o);
    }
    return 0;
  }

} // namespace detail

using websocket_server_type =
  websocketpp::server<detail::asio_with_stub_log<websocketpp::transport::asio::basic_socket::endpoint>>;
//#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
// using websocket_local_server_type = websocketpp::server<detail::asio_local_with_stub_log>;
//#endif
using websocket_server_tls_type =
  websocketpp::server<detail::asio_with_stub_log<websocketpp::transport::asio::tls_socket::endpoint>>;
using ssl_context_ptr = websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>;
using rpc_impl_ptr = std::shared_ptr<class rpc_impl>;

static bool verbose_http_errors = false;

class rpc_impl : public std::enable_shared_from_this<rpc_impl> {
public:
  rpc_impl() = default;

  rpc_impl(const rpc_impl&) = delete;
  rpc_impl(rpc_impl&&) = delete;

  rpc_impl& operator=(const rpc_impl&) = delete;
  rpc_impl& operator=(rpc_impl&&) = delete;

  // key -> priority, url_handler
  map<string, detail::internal_url_handler> url_handlers;
  std::optional<tcp::endpoint> listen_endpoint;
  string access_control_allow_origin;
  string access_control_allow_headers;
  string access_control_max_age;
  bool access_control_allow_credentials = false;
  size_t max_body_size{1024 * 1024};

  websocket_server_type server;

  uint16_t thread_pool_size = 2;
  std::optional<noir::named_thread_pool> thread_pool;
  std::atomic<size_t> bytes_in_flight{0};
  std::atomic<int32_t> requests_in_flight{0};
  size_t max_bytes_in_flight = 0;
  int32_t max_requests_in_flight = -1;
  fc::microseconds max_response_time{30 * 1000};

  std::optional<tcp::endpoint> https_listen_endpoint;
  string https_cert_chain;
  string https_key;
  https_ecdh_curve_t https_ecdh_curve = SECP384R1;

  websocket_server_tls_type https_server;

  //#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
  //  std::optional<asio::local::stream_protocol::endpoint> unix_endpoint;
  //  websocket_local_server_type unix_server;
  //#endif

  using ws_server_type = websocketpp::server<websocketpp::config::asio>;
  std::optional<tcp::endpoint> ws_listen_endpoint;
  ws_server_type ws_server;
  websocket wsocket;

  bool validate_host = true;
  set<string> valid_hosts;

  bool host_port_is_valid(const std::string& header_host_port, const string& endpoint_local_host_port) {
    return !validate_host || header_host_port == endpoint_local_host_port ||
      valid_hosts.find(header_host_port) != valid_hosts.end();
  }

  bool host_is_valid(const std::string& host, const string& endpoint_local_host_port, bool secure) {
    if (!validate_host) {
      return true;
    }

    // normalise the incoming host so that it always has the explicit port
    static auto has_port_expr =
      regex("[^:]:[0-9]+$"); /// ends in :<number> without a preceding colon which implies ipv6
    if (std::regex_search(host, has_port_expr)) {
      return host_port_is_valid(host, endpoint_local_host_port);
    } else {
      // according to RFC 2732 ipv6 addresses should always be enclosed with brackets, so we shouldn't need to special
      // case here
      return host_port_is_valid(
        host + ":" + std::to_string(secure ? websocketpp::uri_default_secure_port : websocketpp::uri_default_port),
        endpoint_local_host_port);
    }
  }

  ssl_context_ptr on_tls_init() {
    ssl_context_ptr ctx =
      websocketpp::lib::make_shared<websocketpp::lib::asio::ssl::context>(asio::ssl::context::sslv23_server);

    try {
      ctx->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 | asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1 |
        asio::ssl::context::single_dh_use);

      ctx->use_certificate_chain_file(https_cert_chain);
      ctx->use_private_key_file(https_key, asio::ssl::context::pem);

      // going for the A+! Do a few more things on the native context to get ECDH in use

      fc::ec_key ecdh = EC_KEY_new_by_curve_name(https_ecdh_curve == SECP384R1 ? NID_secp384r1 : NID_X9_62_prime256v1);
      if (!ecdh)
        throw std::runtime_error("Failed to set NID_secp384r1");
      if (SSL_CTX_set_tmp_ecdh(ctx->native_handle(), (EC_KEY*)ecdh) != 1)
        throw std::runtime_error("Failed to set ECDH PFS");

      if (SSL_CTX_set_cipher_list(ctx->native_handle(),
            "EECDH+ECDSA+AESGCM:EECDH+aRSA+AESGCM:EECDH+ECDSA+SHA384:EECDH+ECDSA+SHA256:AES256:"
            "!DHE:!RSA:!AES128:!RC4:!DES:!3DES:!DSS:!SRP:!PSK:!EXP:!MD5:!LOW:!aNULL:!eNULL") != 1)
        throw std::runtime_error("Failed to set HTTPS cipher list");
    } catch (const std::runtime_error& e) {
      fc_elog(logger, "https server initialization error: ${w}", ("w", e.what()));
    } catch (std::exception& e) {
      fc_elog(logger, "https server initialization error: ${w}", ("w", e.what()));
    }

    return ctx;
  }

  template<class T>
  static void handle_exception(detail::connection_ptr<T> con) {
    string err = "Internal Service error, http: ";
    const auto deadline = fc::time_point::now() + fc::exception::format_time_limit;
    try {
      con->set_status(websocketpp::http::status_code::internal_server_error);
      try {
        throw;
      } catch (const fc::exception& e) {
        err += e.to_detail_string();
        fc_elog(logger, "${e}", ("e", err));
        error_results results{websocketpp::http::status_code::internal_server_error, "Internal Service Error",
          error_results::error_info(e, verbose_http_errors)};
        con->set_body(fc::json::to_string(results, deadline));
      } catch (const std::exception& e) {
        err += e.what();
        fc_elog(logger, "${e}", ("e", err));
        error_results results{websocketpp::http::status_code::internal_server_error, "Internal Service Error",
          error_results::error_info(fc::exception(FC_LOG_MESSAGE(error, e.what())), verbose_http_errors)};
        con->set_body(fc::json::to_string(results, deadline));
      } catch (...) {
        err += "Unknown Exception";
        error_results results{websocketpp::http::status_code::internal_server_error, "Internal Service Error",
          error_results::error_info(fc::exception(FC_LOG_MESSAGE(error, "Unknown Exception")), verbose_http_errors)};
        con->set_body(fc::json::to_string(results, deadline));
      }
    } catch (fc::timeout_exception& e) {
      con->set_body(R"xxx({"message": "Internal Server Error"})xxx");
      fc_elog(
        logger, "Timeout exception ${te} attempting to handle exception: ${e}", ("te", e.to_detail_string())("e", err));
    } catch (...) {
      con->set_body(R"xxx({"message": "Internal Server Error"})xxx");
      fc_elog(logger, "Exception attempting to handle exception: ${e}", ("e", err));
    }
    con->send_http_response();
  }

  template<class T>
  bool allow_host(const typename T::request_type& req, detail::connection_ptr<T> con) {
    bool is_secure = con->get_uri()->get_secure();
    const auto& local_endpoint = con->get_socket().lowest_layer().local_endpoint();
    auto local_socket_host_port = local_endpoint.address().to_string() + ":" + std::to_string(local_endpoint.port());

    const auto& host_str = req.get_header("Host");
    if (host_str.empty() || !host_is_valid(host_str, local_socket_host_port, is_secure)) {
      con->set_status(websocketpp::http::status_code::bad_request);
      return false;
    }
    return true;
  }

  template<typename T>
  void report_429_error(const T& con, const std::string& what) {
    error_results::error_info ei;
    ei.code = websocketpp::http::status_code::too_many_requests;
    ei.name = "Busy";
    ei.what = what;
    error_results results{websocketpp::http::status_code::too_many_requests, "Busy", ei};
    con->set_body(fc::json::to_string(results, fc::time_point::maximum()));
    con->set_status(websocketpp::http::status_code::too_many_requests);
    con->send_http_response();
  }

  template<typename T>
  bool verify_max_bytes_in_flight(const T& con) {
    auto bytes_in_flight_size = bytes_in_flight.load();
    if (bytes_in_flight_size > max_bytes_in_flight) {
      fc_dlog(logger, "429 - too many bytes in flight: ${bytes}", ("bytes", bytes_in_flight_size));
      string what = "Too many bytes in flight: " + std::to_string(bytes_in_flight_size) + ". Try again later.";
      report_429_error(con, what);
      return false;
    }

    return true;
  }

  template<typename T>
  bool verify_max_requests_in_flight(const T& con) {
    if (max_requests_in_flight < 0)
      return true;

    auto requests_in_flight_num = requests_in_flight.load();
    if (requests_in_flight_num > max_requests_in_flight) {
      fc_dlog(logger, "429 - too many requests in flight: ${requests}", ("requests", requests_in_flight_num));
      string what = "Too many requests in flight: " + std::to_string(requests_in_flight_num) + ". Try again later.";
      report_429_error(con, what);
      return false;
    }
    return true;
  }

  /**
   * child struct, implementing abstract connection for various underlying connection types
   * that ties it to an rpc_impl
   *
   * @tparam T - The downstream parameter for the connection_ptr
   */
  template<typename T>
  struct abstract_conn_impl : public detail::abstract_conn {
    abstract_conn_impl(detail::connection_ptr<T> conn, rpc_impl_ptr impl)
      : _conn(std::move(conn)), _impl(std::move(impl)) {
      _impl->requests_in_flight += 1;
    }

    ~abstract_conn_impl() override {
      _impl->requests_in_flight -= 1;
    }

    // No copy constructor and no move
    abstract_conn_impl(const abstract_conn_impl&) = delete;
    abstract_conn_impl(abstract_conn_impl&&) = delete;
    abstract_conn_impl& operator=(const abstract_conn_impl&) = delete;

    abstract_conn_impl& operator=(abstract_conn_impl&&) noexcept = default;

    bool verify_max_bytes_in_flight() override {
      return _impl->verify_max_bytes_in_flight(_conn);
    }

    bool verify_max_requests_in_flight() override {
      return _impl->verify_max_requests_in_flight(_conn);
    }

    void handle_exception() override {
      rpc_impl::handle_exception<T>(_conn);
    }

    void send_response(std::optional<std::string> body, int code) override {
      if (body) {
        _conn->set_body(std::move(*body));
      }
      _conn->set_status(websocketpp::http::status_code::value(code));
      _conn->send_http_response();
    }

    detail::connection_ptr<T> _conn;
    rpc_impl_ptr _impl;
  };

  /**
   * Helper to construct an abstract_conn_impl for a given connection and instance of rpc_impl
   * @tparam T - The downstream parameter for the connection_ptr
   * @param conn - existing connection_ptr<T>
   * @param impl - the owning rpc_impl
   * @return abstract_conn_ptr backed by type specific implementations of the methods
   */
  template<typename T>
  static detail::abstract_conn_ptr make_abstract_conn_ptr(detail::connection_ptr<T> conn, rpc_impl_ptr impl) {
    return std::make_shared<abstract_conn_impl<T>>(std::move(conn), std::move(impl));
  }

  /**
   * Helper type that wraps an object of type T and records its "in flight" size to
   * rpc_impl::bytes_in_flight using RAII semantics
   *
   * @tparam T - the contained Type
   */
  template<typename T>
  struct in_flight {
    in_flight(T&& object, rpc_impl_ptr impl): _object(std::move(object)), _impl(std::move(impl)) {
      _count = detail::in_flight_sizeof(_object);
      _impl->bytes_in_flight += _count;
    }

    ~in_flight() {
      if (_count) {
        _impl->bytes_in_flight -= _count;
      }
    }

    // No copy constructor, but allow move
    in_flight(const in_flight&) = delete;
    in_flight(in_flight&& from): _object(std::move(from._object)), _count(from._count), _impl(std::move(from._impl)) {
      from._count = 0;
    }

    // No copy assignment, but allow move
    in_flight& operator=(const in_flight&) = delete;
    in_flight& operator=(in_flight&& from) {
      _object = std::move(from._object);
      _count = from._count;
      _impl = std::move(from._impl);
      from._count = 0;
    }

    /**
     * const accessor
     * @return const reference to the contained object
     */
    const T& obj() const {
      return _object;
    }

    /**
     * mutable accessor (can be moved from)
     * @return mutable reference to the contained object
     */
    T& obj() {
      return _object;
    }

    T _object;
    size_t _count;
    rpc_impl_ptr _impl;
  };

  /**
   * convenient wrapper to make an in_flight<T>
   */
  template<typename T>
  static auto make_in_flight(T&& object, rpc_impl_ptr impl) {
    return std::make_shared<in_flight<T>>(std::forward<T>(object), std::move(impl));
  }

  /**
   * Make an internal_url_handler that will run the url_handler on the app() thread and then
   * return to the http thread pool for response processing
   *
   * @pre b.size() has been added to bytes_in_flight by caller
   * @param priority - priority to post to the app thread at
   * @param next - the next handler for responses
   * @param my - the rpc_impl
   * @return the constructed internal_url_handler
   */
  static detail::internal_url_handler make_app_thread_url_handler(
    appbase::application& app, int priority, url_handler next, rpc_impl_ptr my) {
    auto next_ptr = std::make_shared<url_handler>(std::move(next));
    return [&app, my = std::move(my), priority, next_ptr = std::move(next_ptr)](
             detail::abstract_conn_ptr conn, string r, string b, url_response_callback then) {
      auto tracked_b = make_in_flight<string>(std::move(b), my);
      if (!conn->verify_max_bytes_in_flight()) {
        return;
      }

      url_response_callback wrapped_then = [tracked_b, then = std::move(then)](int code,
                                             std::optional<fc::variant> resp) { then(code, std::move(resp)); };

      // post to the app thread taking shared ownership of next (via std::shared_ptr),
      // sole ownership of the tracked body and the passed in parameters
      app.post(priority,
        [next_ptr, conn = std::move(conn), r = std::move(r), tracked_b,
          wrapped_then = std::move(wrapped_then)]() mutable {
          try {
            // call the `next` url_handler and wrap the response handler
            (*next_ptr)(std::move(r), std::move(tracked_b->obj()), std::move(wrapped_then));
          } catch (...) {
            conn->handle_exception();
          }
        });
    };
  }

  /**
   * Make an internal_url_handler that will run the url_handler directly
   *
   * @pre b.size() has been added to bytes_in_flight by caller
   * @param next - the next handler for responses
   * @return the constructed internal_url_handler
   */
  static detail::internal_url_handler make_http_thread_url_handler(url_handler next) {
    return
      [next = std::move(next)](const detail::abstract_conn_ptr& conn, string r, string b, url_response_callback then) {
        try {
          next(std::move(r), std::move(b), std::move(then));
        } catch (...) {
          conn->handle_exception();
        }
      };
  }

  /**
   * Construct a lambda appropriate for url_response_callback that will
   * JSON-stringify the provided response
   *
   * @param con - pointer for the connection this response should be sent to
   * @return lambda suitable for url_response_callback
   */
  template<typename T>
  auto make_http_response_handler(const detail::abstract_conn_ptr& abstract_conn_ptr) {
    return [my = shared_from_this(), abstract_conn_ptr](int code, std::optional<fc::variant> response) {
      auto tracked_response = make_in_flight(std::move(response), my);
      if (!abstract_conn_ptr->verify_max_bytes_in_flight()) {
        return;
      }

      // post  back to an HTTP thread to allow the response handler to be called from any thread
      boost::asio::post(my->thread_pool->get_executor(),
        [my, abstract_conn_ptr, code, tracked_response = std::move(tracked_response)]() {
          try {
            if (tracked_response->obj().has_value()) {
              std::string json =
                fc::json::to_string(*tracked_response->obj(), fc::time_point::now() + my->max_response_time);
              auto tracked_json = make_in_flight(std::move(json), my);
              abstract_conn_ptr->send_response(std::move(tracked_json->obj()), code);
            } else {
              abstract_conn_ptr->send_response({}, code);
            }
          } catch (...) {
            abstract_conn_ptr->handle_exception();
          }
        });
    };
  }

  template<class T>
  void handle_http_request(detail::connection_ptr<T> con) {
    try {
      auto& req = con->get_request();

      if (!allow_host<T>(req, con))
        return;

      if (!access_control_allow_origin.empty()) {
        con->append_header("Access-Control-Allow-Origin", access_control_allow_origin);
      }
      if (!access_control_allow_headers.empty()) {
        con->append_header("Access-Control-Allow-Headers", access_control_allow_headers);
      }
      if (!access_control_max_age.empty()) {
        con->append_header("Access-Control-Max-Age", access_control_max_age);
      }
      if (access_control_allow_credentials) {
        con->append_header("Access-Control-Allow-Credentials", "true");
      }

      if (req.get_method() == "OPTIONS") {
        con->set_status(websocketpp::http::status_code::ok);
        return;
      }

      con->append_header("Content-type", "application/json");
      con->defer_http_response();

      auto abstract_conn_ptr = make_abstract_conn_ptr<T>(con, shared_from_this());
      if (!verify_max_bytes_in_flight(con) || !verify_max_requests_in_flight(con))
        return;

      std::string resource = con->get_uri()->get_resource();
      auto handler_itr = url_handlers.find(resource);
      if (handler_itr != url_handlers.end()) {
        std::string body = con->get_request_body();
        handler_itr->second(
          abstract_conn_ptr, std::move(resource), std::move(body), make_http_response_handler<T>(abstract_conn_ptr));
      } else {
        fc_dlog(logger, "404 - not found: ${ep}", ("ep", resource));
        error_results results{websocketpp::http::status_code::not_found, "Not Found",
          error_results::error_info(fc::exception(FC_LOG_MESSAGE(error, "Unknown Endpoint")), verbose_http_errors)};
        con->set_body(fc::json::to_string(results, fc::time_point::now() + max_response_time));
        con->set_status(websocketpp::http::status_code::not_found);
        con->send_http_response();
      }
    } catch (...) {
      handle_exception<T>(con);
    }
  }

  void handle_message(
    websocketpp::server<websocketpp::config::asio>::connection_ptr conn, websocket_server_type::message_ptr msg) {
    std::string resource = conn->get_uri()->get_resource();
    if (wsocket.message_handlers.contains(resource)) {
      wsocket.message_handlers[resource](conn, msg->get_payload(), wsocket.make_message_sender(conn));
    } else {
      conn->send("Unknown Endpoint");
    }
  }

  template<class T>
  void create_server_for_endpoint(const tcp::endpoint& ep, websocketpp::server<detail::asio_with_stub_log<T>>& ws) {
    try {
      ws.clear_access_channels(websocketpp::log::alevel::all);
      ws.init_asio(&thread_pool->get_executor());
      ws.set_reuse_addr(true);
      ws.set_max_http_body_size(max_body_size);
      // captures `this` & ws, my needs to live as long as server is handling requests
      ws.set_http_handler(
        [&](connection_hdl hdl) { handle_http_request<detail::asio_with_stub_log<T>>(ws.get_con_from_hdl(hdl)); });
    } catch (const fc::exception& e) {
      fc_elog(logger, "http: ${e}", ("e", e.to_detail_string()));
    } catch (const std::exception& e) {
      fc_elog(logger, "http: ${e}", ("e", e.what()));
    } catch (...) {
      fc_elog(logger, "error thrown from http io service");
    }
  }

  void add_aliases_for_endpoint(const tcp::endpoint& ep, const string& host, const string& port) {
    auto resolved_port_str = std::to_string(ep.port());
    valid_hosts.emplace(host + ":" + port);
    valid_hosts.emplace(host + ":" + resolved_port_str);
  }

  void create_ws_server_for_endpoint(const tcp::endpoint& ep, ws_server_type& ws) {
    try {
      ws.clear_access_channels(websocketpp::log::alevel::all);
      ws.set_access_channels(websocketpp::log::alevel::all);
      ws.init_asio(&thread_pool->get_executor());
      ws.set_message_handler([&](websocketpp::connection_hdl hdl, ws_server_type::message_ptr msg) {
        handle_message(ws.get_con_from_hdl(hdl), msg);
      });
    } catch (const fc::exception& e) {
      fc_elog(logger, "ws: ${e}", ("e", e.to_detail_string()));
    } catch (const std::exception& e) {
      fc_elog(logger, "ws: ${e}", ("e", e.what()));
    } catch (...) {
      fc_elog(logger, "error thrown from ws io service");
    }
  }
};

//#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
// template<>
// bool rpc_impl::allow_host<detail::asio_local_with_stub_log>(const detail::asio_local_with_stub_log::request_type&
// req, websocketpp::server<detail::asio_local_with_stub_log>::connection_ptr con) {
//  return true;
//}
//#endif

rpc::rpc(appbase::application& app): plugin(app), my(new rpc_impl()) {}
rpc::~rpc() = default;

void rpc::set_program_options(CLI::App& config) {
  auto rpc_options = config.add_section("rpc",
    "###############################################\n"
    "###        RPC Configuration Options        ###\n"
    "###############################################");

  //#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
  //  if (current_rpc_defaults.default_unix_socket_path.length())
  //    rpc_options->add_option("unix-socket-path",
  //        "The filename (relative to data-dir) to create a unix socket for HTTP RPC; set blank to disable.")
  //        ->force_callback()
  //        ->default_val(current_rpc_defaults.default_unix_socket_path);
  //  else
  //    rpc_options->add_option("unix-socket-path",
  //        "The filename (relative to data-dir) to create a unix socket for HTTP RPC; set blank to disable.");
  //#endif

  if (current_rpc_defaults.default_http_port)
    rpc_options
      ->add_option(
        "--http-server-address", "The local IP and port to listen for incoming http connections; set blank to disable.")
      ->default_val("127.0.0.1:" + std::to_string(current_rpc_defaults.default_http_port));
  else
    rpc_options->add_option("--http-server-address",
      "The local IP and port to listen for incoming http connections; leave blank to disable.");

  rpc_options->add_option("--ws-server-address",
    "The local IP and port to listen for incoming ws connections; leave blank to disable.");

  rpc_options->add_option("--https-server-address",
    "The local IP and port to listen for incoming https connections; leave blank to disable.");
  rpc_options->add_option("--https-certificate-chain-file",
    "Filename with the certificate chain to present on https connections. PEM format. Required for https.");
  rpc_options->add_option(
    "--https-private-key-file", "Filename with https private key in PEM format. Required for https");
  rpc_options
    ->add_option(
      "--https-ecdh-curve", my->https_ecdh_curve, "Configure https ECDH curve to use: secp384r1 or prime256v1")
    ->transform(CLI::CheckedTransformer(https_ecdh_curve_map, CLI::ignore_case))
    ->default_val(https_ecdh_curve_t::SECP384R1);

  rpc_options->add_option("--max-body-size", "The maximum body size in bytes allowed for incoming RPC requests")
    ->default_val(1024 * 1024);
  rpc_options
    ->add_option("--http-max-bytes-in-flight-mb",
      "Maximum size in megabytes rpc should use for processing http requests. 429 error response when exceeded.")
    ->default_val(500);
  rpc_options
    ->add_option("--http-max-in-flight-requests",
      "Maximum number of requests rpc should use for processing http requests. 429 error response when exceeded.")
    ->default_val(-1);
  rpc_options->add_option("--http-max-response-time-ms", "Maximum time for processing a request.")->default_val(30);
  rpc_options->add_option("--verbose-http-errors", "Append the error log to HTTP responses")->default_val(false);
  rpc_options
    ->add_option("--http-validate-host", "If set to false, then any incoming \"Host\" header is considered valid")
    ->default_val(true);
  rpc_options
    ->add_option("--http-alias",
      "Additionally acceptable values for the \"Host\" header of incoming HTTP requests, can be specified multiple "
      "times. Includes http/s_server_address by default.")
    ->take_all();
  rpc_options->add_option("--http-threads", "Number of worker threads in http thread pool")->default_val(2);
  rpc_options->add_option("--access-control-allow-origin", my->access_control_allow_origin,
    "Specify the Access-Control-Allow-Origin to be returned on each request.");
  rpc_options->add_option("--access-control-allow-headers", my->access_control_allow_headers,
    "Specify the Access-Control-Allow-Headers to be returned on each request.");
  rpc_options->add_option("--access-control-max-age", my->access_control_max_age,
    "Specify the Access-Control-Max-Age to be returned on each request.");
  rpc_options
    ->add_option("--access-control-allow-credentials", my->access_control_allow_credentials,
      "Specify if Access-Control-Allow-Credentials: true should be returned on each request.")
    ->force_callback()
    ->default_val(false);
}

void rpc::plugin_initialize(const CLI::App& config) {
  try {
    auto rpc_options = config.get_subcommand("rpc");

    my->validate_host = rpc_options->get_option("--http-validate-host")->as<bool>();
    if (rpc_options->count("--http-alias")) {
      const auto& aliases = rpc_options->get_option("--http-alias")->as<vector<string>>();
      my->valid_hosts.insert(aliases.begin(), aliases.end());
    }

    tcp::resolver resolver(app.io_context());
    if (rpc_options->get_option("--http-server-address")->as<string>().length()) {
      string lipstr = rpc_options->get_option("--http-server-address")->as<string>();
      string host = lipstr.substr(0, lipstr.find(':'));
      string port = lipstr.substr(host.size() + 1, lipstr.size());
      try {
        my->listen_endpoint = *resolver.resolve(tcp::v4(), host, port);
        ilog("configured http to listen on ${h}:${p}", ("h", host)("p", port));
      } catch (const boost::system::system_error& ec) {
        elog("failed to configure http to listen on ${h}:${p} (${m})", ("h", host)("p", port)("m", ec.what()));
      }

      // add in resolved hosts and ports as well
      if (my->listen_endpoint) {
        my->add_aliases_for_endpoint(*my->listen_endpoint, host, port);
      }
    }

    //#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
    //    if(rpc_options->count("unix-socket-path") &&
    //    !rpc_options->get_option("unix-socket-path")->as<string>().empty()) {
    //      boost::filesystem::path sock_path = rpc_options->get_option("unix-socket-path")->as<string>();
    //      if (sock_path.is_relative())
    //        sock_path = app().data_dir() / sock_path;
    //      my->unix_endpoint = asio::local::stream_protocol::endpoint(sock_path.string());
    //    }
    //#endif

    if (rpc_options->count("--https-server-address") &&
      rpc_options->get_option("--https-server-address")->as<string>().length()) {
      if (!rpc_options->count("--https-certificate-chain-file") ||
        rpc_options->get_option("--https-certificate-chain-file")->as<string>().empty()) {
        elog("https-certificate-chain-file is required for HTTPS");
        return;
      }
      if (!rpc_options->count("--https-private-key-file") ||
        rpc_options->get_option("--https-private-key-file")->as<string>().empty()) {
        elog("https-private-key-file is required for HTTPS");
        return;
      }

      string lipstr = rpc_options->get_option("--https-server-address")->as<string>();
      string host = lipstr.substr(0, lipstr.find(':'));
      string port = lipstr.substr(host.size() + 1, lipstr.size());
      try {
        my->https_listen_endpoint = *resolver.resolve(tcp::v4(), host, port);
        ilog("configured https to listen on ${h}:${p} (TLS configuration will be validated momentarily)",
          ("h", host)("p", port));
        my->https_cert_chain = rpc_options->get_option("--https-certificate-chain-file")->as<string>();
        my->https_key = rpc_options->get_option("--https-private-key-file")->as<string>();
      } catch (const boost::system::system_error& ec) {
        elog("failed to configure https to listen on ${h}:${p} (${m})", ("h", host)("p", port)("m", ec.what()));
      }

      // add in resolved hosts and ports as well
      if (my->https_listen_endpoint) {
        my->add_aliases_for_endpoint(*my->https_listen_endpoint, host, port);
      }
    }

    if (rpc_options->count("--ws-server-address") && rpc_options->get_option("--ws-server-address")->as<string>().length()) {
      string lipstr = rpc_options->get_option("--ws-server-address")->as<string>();
      string host = lipstr.substr(0, lipstr.find(':'));
      string port = lipstr.substr(host.size() + 1, lipstr.size());
      try {
        my->ws_listen_endpoint = *resolver.resolve(tcp::v4(), host, port);
        ilog("configured ws to listen on ${h}:${p}", ("h", host)("p", port));
      } catch (const boost::system::system_error& ec) {
        elog("failed to configure ws to listen on ${h}:${p} (${m})", ("h", host)("p", port)("m", ec.what()));
      }

      // add in resolved hosts and ports as well
      if (my->ws_listen_endpoint) {
        my->add_aliases_for_endpoint(*my->ws_listen_endpoint, host, port);
      }
    }

    my->max_body_size = rpc_options->get_option("--max-body-size")->as<uint32_t>();
    verbose_http_errors = rpc_options->get_option("--verbose-http-errors")->as<bool>();

    my->thread_pool_size = rpc_options->get_option("--http-threads")->as<uint16_t>();
    if (my->thread_pool_size <= 0)
      throw std::runtime_error("number of http-threads must be greater than 0");

    my->max_bytes_in_flight = rpc_options->get_option("--http-max-bytes-in-flight-mb")->as<uint32_t>() * 1024 * 1024;
    my->max_requests_in_flight = rpc_options->get_option("--http-max-in-flight-requests")->as<int32_t>();
    my->max_response_time =
      fc::microseconds(rpc_options->get_option("--http-max-response-time-ms")->as<uint32_t>() * 1000);

    // watch out for the returns above when adding new code here
  }
  FC_LOG_AND_RETHROW()
}

void rpc::plugin_startup() {
  handle_sighup(); // setup logging
  app.post(priority::high, [this]() {
    try {
      my->thread_pool.emplace("rpc", my->thread_pool_size);
      if (my->listen_endpoint) {
        try {
          my->create_server_for_endpoint(*my->listen_endpoint, my->server);

          fc_ilog(logger, "start listening for http requests");
          my->server.listen(*my->listen_endpoint);
          my->server.start_accept();
        } catch (const fc::exception& e) {
          fc_elog(logger, "http service failed to start: ${e}", ("e", e.to_detail_string()));
          throw;
        } catch (const std::exception& e) {
          fc_elog(logger, "http service failed to start: ${e}", ("e", e.what()));
          throw;
        } catch (...) {
          fc_elog(logger, "error thrown from http io service");
          throw;
        }
      }

      //#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
      //      if(my->unix_endpoint) {
      //        try {
      //          my->unix_server.clear_access_channels(websocketpp::log::alevel::all);
      //          my->unix_server.init_asio(&my->thread_pool->get_executor());
      //          my->unix_server.set_max_http_body_size(my->max_body_size);
      //          my->unix_server.listen(*my->unix_endpoint);
      //          // captures `this`, my needs to live as long as unix_server is handling requests
      //          my->unix_server.set_http_handler([this](connection_hdl hdl) {
      //            my->handle_http_request<detail::asio_local_with_stub_log>(my->unix_server.get_con_from_hdl(std::move(hdl)));
      //          });
      //          my->unix_server.start_accept();
      //        } catch (const fc::exception& e){
      //          fc_elog(logger, "unix socket service (${path}) failed to start: ${e}", ("e",
      //          e.to_detail_string())("path",my->unix_endpoint->path())); throw;
      //        } catch (const std::exception& e){
      //          fc_elog(logger, "unix socket service (${path}) failed to start: ${e}", ("e",
      //          e.what())("path",my->unix_endpoint->path())); throw;
      //        } catch (...) {
      //          fc_elog(logger, "error thrown from unix socket (${path}) io service",
      //          ("path",my->unix_endpoint->path())); throw;
      //        }
      //      }
      //#endif

      if (my->https_listen_endpoint) {
        try {
          my->create_server_for_endpoint(*my->https_listen_endpoint, my->https_server);
          my->https_server.set_tls_init_handler(
            [this](const websocketpp::connection_hdl& hdl) -> ssl_context_ptr { return my->on_tls_init(); });

          fc_ilog(logger, "start listening for https requests");
          my->https_server.listen(*my->https_listen_endpoint);
          my->https_server.start_accept();
        } catch (const fc::exception& e) {
          fc_elog(logger, "https service failed to start: ${e}", ("e", e.to_detail_string()));
          throw;
        } catch (const std::exception& e) {
          fc_elog(logger, "https service failed to start: ${e}", ("e", e.what()));
          throw;
        } catch (...) {
          fc_elog(logger, "error thrown from https io service");
          throw;
        }
      }

      if (my->ws_listen_endpoint) {
        try {
          my->create_ws_server_for_endpoint(*my->ws_listen_endpoint, my->ws_server);
          fc_ilog(logger, "start listening for ws requests");
          my->ws_server.listen(*my->ws_listen_endpoint);
          my->ws_server.start_accept();
        } catch (const fc::exception& e) {
          fc_elog(logger, "http service failed to start: ${e}", ("e", e.to_detail_string()));
          throw;
        } catch (const std::exception& e) {
          fc_elog(logger, "http service failed to start: ${e}", ("e", e.what()));
          throw;
        } catch (...) {
          fc_elog(logger, "error thrown from http io service");
          throw;
        }
      }
    } catch (...) {
      fc_elog(logger, "rpc startup fails, shutting down");
      app.quit();
    }
  });
}

void rpc::handle_sighup() {
  fc::logger::update(logger_name, logger);
}

void rpc::plugin_shutdown() {
  if (my->server.is_listening())
    my->server.stop_listening();
  if (my->https_server.is_listening())
    my->https_server.stop_listening();
  if (my->ws_server.is_listening())
    my->ws_server.stop_listening();
  //#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
  //  if(my->unix_server.is_listening())
  //    my->unix_server.stop_listening();
  //#endif

  if (my->thread_pool) {
    my->thread_pool->stop();
    my->thread_pool.reset();
  }

  // release rpc_impl_ptr shared_ptrs captured in url handlers
  my->url_handlers.clear();

  app.post(0, [me = my]() {}); // keep my pointer alive until queue is drained
}

void rpc::add_handler(const string& url, const url_handler& handler, int priority) {
  fc_ilog(logger, "add api url: ${c}", ("c", url));
  my->url_handlers[url] = my->make_app_thread_url_handler(app, priority, handler, my);
}

void rpc::add_async_handler(const string& url, const url_handler& handler) {
  fc_ilog(logger, "add api url: ${c}", ("c", url));
  my->url_handlers[url] = my->make_http_thread_url_handler(handler);
}

void rpc::handle_exception(const char* api_name, const char* call_name, const string& body, url_response_callback cb) {
  try {
    try {
      throw;
    } catch (fc::eof_exception& e) {
      error_results results{422, "Unprocessable Entity", error_results::error_info(e, verbose_http_errors)};
      cb(422, fc::variant(results));
      fc_elog(logger, "Unable to parse arguments to ${api}.${call}", ("api", api_name)("call", call_name));
      fc_dlog(logger, "Bad arguments: ${args}", ("args", body));
    } catch (fc::exception& e) {
      error_results results{500, "Internal Service Error", error_results::error_info(e, verbose_http_errors)};
      cb(500, fc::variant(results));
      fc_dlog(logger, "Exception while processing ${api}.${call}: ${e}",
        ("api", api_name)("call", call_name)("e", e.to_detail_string()));
    } catch (std::exception& e) {
      error_results results{500, "Internal Service Error",
        error_results::error_info(fc::exception(FC_LOG_MESSAGE(error, e.what())), verbose_http_errors)};
      cb(500, fc::variant(results));
      fc_elog(
        logger, "STD Exception encountered while processing ${api}.${call}", ("api", api_name)("call", call_name));
      fc_dlog(logger, "Exception Details: ${e}", ("e", e.what()));
    } catch (...) {
      error_results results{500, "Internal Service Error",
        error_results::error_info(fc::exception(FC_LOG_MESSAGE(error, "Unknown Exception")), verbose_http_errors)};
      cb(500, fc::variant(results));
      fc_elog(
        logger, "Unknown Exception encountered while processing ${api}.${call}", ("api", api_name)("call", call_name));
    }
  } catch (...) {
    std::cerr << "Exception attempting to handle exception for " << api_name << "." << call_name << std::endl;
  }
}

bool rpc::is_on_loopback() const {
  return (!my->listen_endpoint || my->listen_endpoint->address().is_loopback()) &&
    (!my->https_listen_endpoint || my->https_listen_endpoint->address().is_loopback());
}

bool rpc::is_secure() const {
  return (!my->listen_endpoint || my->listen_endpoint->address().is_loopback());
}

bool rpc::verbose_errors() const {
  return verbose_http_errors;
}

rpc::get_supported_apis_result rpc::get_supported_apis() const {
  get_supported_apis_result result;

  for (const auto& handler : my->url_handlers) {
    if (handler.first != "/v1/node/get_supported_apis")
      result.apis.emplace_back(handler.first);
  }

  return result;
}

fc::microseconds rpc::get_max_response_time() const {
  return my->max_response_time;
}

std::istream& operator>>(std::istream& in, https_ecdh_curve_t& curve) {
  std::string s;
  in >> s;
  if (s == "secp384r1")
    curve = SECP384R1;
  else if (s == "prime256v1")
    curve = PRIME256V1;
  else
    in.setstate(std::ios_base::failbit);
  return in;
}

std::ostream& operator<<(std::ostream& osm, https_ecdh_curve_t curve) {
  if (curve == SECP384R1) {
    osm << "secp384r1";
  } else if (curve == PRIME256V1) {
    osm << "prime256v1";
  }

  return osm;
}

} // namespace noir::rpc
