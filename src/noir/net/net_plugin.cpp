#include <noir/net/net_plugin.h>
#include <noir/net/consensus/block.h>
#include <noir/net/consensus/tx.h>
#include <noir/net/queued_buffer.h>
#include <noir/net/thread_util.h>
#include <noir/net/buffer_factory.h>

#include <appbase/application.hpp>

#include <fc/network/message_buffer.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/log/trace.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/exception/exception.hpp>
#include <fc/static_variant.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/steady_timer.hpp>

#include <atomic>
#include <shared_mutex>

namespace noir::net {

// todo - register here or in main?
//static appbase::abstract_plugin &_net_plugin = appbase::app().register_plugin<net_plugin>();

using std::vector;

using boost::asio::ip::tcp;
using boost::asio::ip::address_v4;
using boost::asio::ip::host_name;
using boost::multi_index_container;

using fc::time_point;
using fc::time_point_sec;



class connection : public std::enable_shared_from_this<connection> {
 public:
  explicit connection(string endpoint);
  connection();

  ~connection() {}

  bool start_session();

  bool socket_is_open() const { return socket_open.load(); } // thread safe, atomic
  const string &peer_address() const { return peer_addr; } // thread safe, const

  void set_connection_type(const string &peer_addr);
  bool is_transactions_only_connection() const { return connection_type == transactions_only; }
  bool is_blocks_only_connection() const { return connection_type == blocks_only; }
  void set_heartbeat_timeout(std::chrono::milliseconds msec) {
    std::chrono::system_clock::duration dur = msec;
    hb_timeout = dur.count();
  }

 private:
  static const string unknown;

  void update_endpoints();

//  std::optional<peer_sync_state> peer_requested;  // this peer is requesting info from us

  std::atomic<bool> socket_open{false};

  const string peer_addr;
  enum connection_types : char {
    both,
    transactions_only,
    blocks_only
  };

  std::atomic<connection_types> connection_type{both};

 public:
  boost::asio::io_context::strand strand;
  std::shared_ptr<tcp::socket> socket; // only accessed through strand after construction

  fc::message_buffer<1024 * 1024> pending_message_buffer;
  std::atomic<std::size_t> outstanding_read_bytes{0}; // accessed only from strand threads

  queued_buffer buffer_queue;

  std::atomic<uint32_t> trx_in_progress_size{0};
  const uint32_t connection_id;
  int16_t sent_handshake_count = 0;
  std::atomic<bool> connecting{true};
  std::atomic<bool> syncing{false};

  std::atomic<uint16_t> protocol_version = 0;
  uint16_t consecutive_rejected_blocks = 0;
//  block_status_monitor block_status_monitor_;
  std::atomic<uint16_t> consecutive_immediate_connection_close = 0;

  std::mutex response_expected_timer_mtx;
  boost::asio::steady_timer response_expected_timer;

  std::atomic<go_away_reason> no_retry{no_reason};

  mutable std::mutex conn_mtx; //< mtx for last_req .. local_endpoint_port
//  std::optional<request_message> last_req;
  handshake_message last_handshake_recv;
  handshake_message last_handshake_sent;
  block_id_type fork_head;
  uint32_t fork_head_num{0};
  fc::time_point last_close;
  fc::sha256 conn_node_id;
  string remote_endpoint_ip;
  string remote_endpoint_port;
  string local_endpoint_ip;
  string local_endpoint_port;

  connection_status get_status() const;

  /** \name Peer Timestamps
   *  Time message handling
   *  @{
   */
  // Members set from network data
  tstamp org{0};          //!< originate timestamp
  tstamp rec{0};          //!< receive timestamp
  tstamp dst{0};          //!< destination timestamp
  tstamp xmt{0};          //!< transmit timestamp
  /** @} */
  // timestamp for the lastest message
  tstamp latest_msg_time{0};
  tstamp hb_timeout;

  bool connected();
  bool current();

  /// @param reconnect true if we should try and reconnect immediately after close
  /// @param shutdown true only if plugin is shutting down
  void close(bool reconnect = true, bool shutdown = false);
 private:
  static void _close(connection *self, bool reconnect, bool shutdown); // for easy capture

  bool process_next_block_message(uint32_t message_length);
  bool process_next_trx_message(uint32_t message_length);
 public:

  bool populate_handshake(handshake_message &hello, bool force);

  bool resolve_and_connect();
  void connect(const std::shared_ptr<tcp::resolver> &resolver, tcp::resolver::results_type endpoints);
  void start_read_message();

  /** \brief Process the next message from the pending message buffer
   *
   * Process the next message from the pending_message_buffer.
   * message_length is the already determined length of the data
   * part of the message that will handle the message.
   * Returns true is successful. Returns false if an error was
   * encountered unpacking or processing the message.
   */
  bool process_next_message(uint32_t message_length);

  void send_handshake(bool force = false);

  /** \name Peer Timestamps
   *  Time message handling
   */
  /**  \brief Check heartbeat time and send Time_message
   */
  void check_heartbeat(tstamp current_time);
  /**  \brief Populate and queue time_message
   */
  void send_time();
  /** \brief Populate and queue time_message immediately using incoming time_message
   */
  void send_time(const time_message &msg);
  /** \brief Read system time and convert to a 64 bit integer.
   *
   * There are only two calls on this routine in the program.  One
   * when a packet arrives from the network and the other when a
   * packet is placed on the send queue.  Calls the kernel time of
   * day routine and converts to a (at least) 64 bit integer.
   */
  static tstamp get_time() {
    return std::chrono::system_clock::now().time_since_epoch().count();
  }
  /** @} */

  const string peer_name();

  void blk_send_branch(const block_id_type &msg_head_id);
  void blk_send_branch_impl(uint32_t msg_head_num, uint32_t lib_num, uint32_t head_num);
  void blk_send(const block_id_type &blkid);
  void stop_send();

  void enqueue(const net_message &msg);
//  void enqueue_block(const signed_block_ptr &sb, bool to_sync_queue = false);
  void enqueue_buffer(const std::shared_ptr<std::vector<char>> &send_buffer,
                      go_away_reason close_after_send,
                      bool to_sync_queue = false);
//  void cancel_sync(go_away_reason);
  void flush_queues();
  bool enqueue_sync_block();
  void request_sync_blocks(uint32_t start, uint32_t end);

  void cancel_wait();
  void sync_wait();
  void fetch_wait();
  void sync_timeout(boost::system::error_code ec);
  void fetch_timeout(boost::system::error_code ec);

  void queue_write(const std::shared_ptr<std::vector<char>>
                   &buff,
                   std::function<void(boost::system::error_code, std::size_t)> callback,
                   bool to_sync_queue = false
  );
  void do_queue_write();

  static bool is_valid(const handshake_message &msg);

  void handle_message(const handshake_message &msg);
//  void handle_message(const chain_size_message &msg);
  void handle_message(const go_away_message &msg);
  /** \name Peer Timestamps
   *  Time message handling
   *  @{
   */
  /** \brief Process time_message
   *
   * Calculate offset, delay and dispersion.  Note carefully the
   * implied processing.  The first-order difference is done
   * directly in 64-bit arithmetic, then the result is converted
   * to floating double.  All further processing is in
   * floating-double arithmetic with rounding done by the hardware.
   * This is necessary in order to avoid overflow and preserve precision.
   */
  void handle_message(const time_message &msg);
  /** @} */
//  void handle_message(const notice_message &msg);
//  void handle_message(const request_message &msg);
//  void handle_message(const sync_request_message &msg);
//  void handle_message(const signed_block &msg) = delete; // signed_block_ptr overload used instead
//  void handle_message(const block_id_type &id, signed_block_ptr msg);
//  void handle_message(const packed_transaction &msg) = delete; // packed_transaction_ptr overload used instead
//  void handle_message(packed_transaction_ptr msg);

//  void process_signed_block(const block_id_type &id, signed_block_ptr msg);

  fc::variant_object get_logger_variant() {
    fc::mutable_variant_object mvo;
    mvo("_name", peer_name());
    std::lock_guard<std::mutex> g_conn(conn_mtx);
    mvo("_id", conn_node_id)
        ("_sid", conn_node_id.str().substr(0, 7))
        ("_ip", remote_endpoint_ip)
        ("_port", remote_endpoint_port)
        ("_lip", local_endpoint_ip)
        ("_lport", local_endpoint_port);
    return mvo;
  }
};

using connection_ptr = std::shared_ptr<connection>;
using connection_wptr = std::weak_ptr<connection>;

const string connection::unknown = "<unknown>";

// called from connection strand
struct msg_handler : public fc::visitor<void> {
  connection_ptr c;
  explicit msg_handler(const connection_ptr &conn) : c(conn) {}

//  template<typename T>
//  void operator()( const T& ) const {
//    EOS_ASSERT( false, plugin_config_exception, "Not implemented, call handle_message directly instead" );
//  }

  void operator()(const handshake_message &msg) const {
    // continue call to handle_message on connection strand
    dlog("handle handshake_message");
    c->handle_message(msg);
  }

  void operator()(const go_away_message &msg) const {
    // continue call to handle_message on connection strand
    dlog("handle go_away_message");
    c->handle_message(msg);
  }

  void operator()(const time_message &msg) const {
    // continue call to handle_message on connection strand
    dlog("handle time_message");
    c->handle_message(msg);
  }

  void operator()(const proposal_message &msg) const {
  }

  void operator()(const block_part_message &msg) const {
  }

  void operator()(const vote_message &msg) const {
  }
};



//------------------------------------------------------------------------
// net_plugin_impl
//------------------------------------------------------------------------
class net_plugin_impl : public std::enable_shared_from_this<net_plugin_impl> {
 public:
  unique_ptr<tcp::acceptor> acceptor;
  std::atomic<uint32_t> current_connection_id{0};

//  unique_ptr<sync_manager> sync_master;
//  unique_ptr<dispatch_manager> dispatcher;

  /**
   * Thread safe, only updated in plugin initialize
   *  @{
   */
  string p2p_address;
  string p2p_server_address;

  vector<string> supplied_peers;
  vector<public_key_type> allowed_peers; ///< peer keys allowed to connect
  std::map<public_key_type,
           private_key_type>
      private_keys; ///< overlapping with producer keys, also authenticating non-producing nodes
  enum possible_connections : char {
    None = 0,
    Producers = 1 << 0,
    Specified = 1 << 1,
    Any = 1 << 2
  };
  possible_connections allowed_connections{None};

  boost::asio::steady_timer::duration connector_period{0};
  boost::asio::steady_timer::duration txn_exp_period{0};
  boost::asio::steady_timer::duration resp_expected_period{0};
  std::chrono::milliseconds keepalive_interval{std::chrono::milliseconds{32 * 1000}};
  std::chrono::milliseconds heartbeat_timeout{keepalive_interval * 2};

  int max_cleanup_time_ms = 0;
  uint32_t max_client_count = 0;
  uint32_t max_nodes_per_host = 1;
  bool p2p_accept_transactions = true;
  bool p2p_reject_incomplete_blocks = true;

  /// Peer clock may be no more than 1 second skewed from our clock, including network latency.
  const std::chrono::system_clock::duration peer_authentication_interval{std::chrono::seconds{1}};

//  chain_id_type chain_id;
  fc::sha256 node_id;
  string user_agent_name;

//  chain_plugin *chain_plug = nullptr;
//  producer_plugin *producer_plug = nullptr;
  bool use_socket_read_watermark = false;
  /** @} */

  mutable std::shared_mutex connections_mtx;
  std::set<connection_ptr> connections;

  std::mutex connector_check_timer_mtx;
  unique_ptr<boost::asio::steady_timer> connector_check_timer;
  int connector_checks_in_flight{0};

  std::mutex expire_timer_mtx;
  unique_ptr<boost::asio::steady_timer> expire_timer;

  std::mutex keepalive_timer_mtx;
  unique_ptr<boost::asio::steady_timer> keepalive_timer;

  std::atomic<bool> in_shutdown{false};

//  compat::channels::transaction_ack::channel_type::handle incoming_transaction_ack_subscription;

  uint16_t thread_pool_size = 2;
  std::optional<named_thread_pool> thread_pool;

 private:
  mutable std::mutex chain_info_mtx; // protects chain_*
  uint32_t chain_lib_num{0};
  uint32_t chain_head_blk_num{0};
  uint32_t chain_fork_head_blk_num{0};
  block_id_type chain_lib_id;
  block_id_type chain_head_blk_id;
  block_id_type chain_fork_head_blk_id;

 public:
  void update_chain_info();
  //         lib_num, head_block_num, fork_head_blk_num, lib_id, head_blk_id, fork_head_blk_id
  std::tuple<uint32_t, uint32_t, uint32_t, block_id_type, block_id_type, block_id_type> get_chain_info() const;

  void start_listen_loop();

  void on_accepted_block(const consensus::block_ptr &bs);
//  void on_pre_accepted_block(const signed_block_ptr &bs);
//  void transaction_ack(const std::pair<fc::exception_ptr, transaction_metadata_ptr> &);
//  void on_irreversible_block(const block_state_ptr &blk);

  void start_conn_timer(boost::asio::steady_timer::duration du, std::weak_ptr<connection> from_connection);
  void start_expire_timer();
  void start_monitors();

  void expire();
  void connection_monitor(std::weak_ptr<connection> from_connection, bool reschedule);
  /** \name Peer Timestamps
   *  Time message handling
   *  @{
   */
  /** \brief Peer heartbeat ticker.
   */
  void ticker();
  /** @} */
  /** \brief Determine if a peer is allowed to connect.
   *
   * Checks current connection mode and key authentication.
   *
   * \return False if the peer should not connect, true otherwise.
   */
  bool authenticate_peer(const handshake_message &msg) const;
  /** \brief Retrieve public key used to authenticate with peers.
   *
   * Finds a key to use for authentication.  If this node is a producer, use
   * the front of the producer key map.  If the node is not a producer but has
   * a configured private key, use it.  If the node is neither a producer nor has
   * a private key, returns an empty key.
   *
   * \note On a node with multiple private keys configured, the key with the first
   *       numerically smaller byte will always be used.
   */
  public_key_type get_authentication_key() const;
  /** \brief Returns a signature of the digest using the corresponding private key of the signer.
   *
   * If there are no configured private keys, returns an empty signature.
   */
  signature_type sign_compact(const public_key_type &signer, const fc::sha256 &digest) const;

  constexpr static uint16_t to_protocol_version(uint16_t v);

  connection_ptr find_connection(const string &host) const; // must call with held mutex
};

static net_plugin_impl *my_impl;

template<typename Function>
void for_each_connection(Function f) {
  std::shared_lock<std::shared_mutex> g(my_impl->connections_mtx);
  for (auto &c: my_impl->connections) {
    if (!f(c)) return;
  }
}

void net_plugin_impl::start_monitors() {
  {
    std::lock_guard<std::mutex> g(connector_check_timer_mtx);
    connector_check_timer.reset(new boost::asio::steady_timer(my_impl->thread_pool->get_executor()));
  }
//  {
//    std::lock_guard<std::mutex> g( expire_timer_mtx );
//    expire_timer.reset( new boost::asio::steady_timer( my_impl->thread_pool->get_executor() ) );
//  }
  start_conn_timer(connector_period, std::weak_ptr<connection>());
//  start_expire_timer(); // todo - we only check connection expiration for now
}

void net_plugin_impl::start_conn_timer(boost::asio::steady_timer::duration du,
                                       std::weak_ptr<connection> from_connection) {
  if (in_shutdown) return;
  std::lock_guard<std::mutex> g(connector_check_timer_mtx);
  ++connector_checks_in_flight;
  connector_check_timer->expires_from_now(du);
  connector_check_timer->async_wait([my = shared_from_this(), from_connection](boost::system::error_code ec) {
    std::unique_lock<std::mutex> g(my->connector_check_timer_mtx);
    int num_in_flight = --my->connector_checks_in_flight;
    g.unlock();
    if (!ec) {
      my->connection_monitor(from_connection, num_in_flight == 0);
    } else {
      if (num_in_flight == 0) {
        if (my->in_shutdown) return;
        elog("Error from connection check monitor: ${m}", ("m", ec.message()));
        my->start_conn_timer(my->connector_period, std::weak_ptr<connection>());
      }
    }
  });
}

void net_plugin_impl::connection_monitor(std::weak_ptr<connection> from_connection, bool reschedule) {
  auto max_time = fc::time_point::now();
  max_time += fc::milliseconds(max_cleanup_time_ms);
  auto from = from_connection.lock();
  std::unique_lock<std::shared_mutex> g(connections_mtx);
  auto it = (from ? connections.find(from) : connections.begin());
  if (it == connections.end()) it = connections.begin();
  size_t num_rm = 0, num_clients = 0, num_peers = 0;
  while (it != connections.end()) {
    if (fc::time_point::now() >= max_time) {
      connection_wptr wit = *it;
      g.unlock();
      dlog("Exiting connection monitor early, ran out of time: ${t}", ("t", max_time - fc::time_point::now()));
      if (reschedule) {
        start_conn_timer(std::chrono::milliseconds(1), wit); // avoid exhausting
      }
      return;
    }
    (*it)->peer_address().empty() ? ++num_clients : ++num_peers;
    if (!(*it)->socket_is_open() && !(*it)->connecting) {
      if (!(*it)->peer_address().empty()) {
        if (!(*it)->resolve_and_connect()) {
          it = connections.erase(it);
          --num_peers;
          ++num_rm;
          continue;
        }
      } else {
        --num_clients;
        ++num_rm;
        it = connections.erase(it);
        continue;
      }
    }
    ++it;
  }
  g.unlock();
  if (num_clients > 0 || num_peers > 0)
    ilog("p2p client connections: ${num}/${max}, peer connections: ${pnum}/${pmax}",
         ("num", num_clients)("max", max_client_count)("pnum", num_peers)("pmax", supplied_peers.size()));
  dlog("connection monitor, removed ${n} connections", ("n", num_rm));
  if (reschedule) {
    start_conn_timer(connector_period, std::weak_ptr<connection>());
  }
}

void net_plugin_impl::update_chain_info() {
//  controller& cc = chain_plug->chain();
//  std::lock_guard<std::mutex> g( chain_info_mtx );
//  chain_lib_num = cc.last_irreversible_block_num();
//  chain_lib_id = cc.last_irreversible_block_id();
//  chain_head_blk_num = cc.head_block_num();
//  chain_head_blk_id = cc.head_block_id();
//  chain_fork_head_blk_num = cc.fork_db_pending_head_block_num();
//  chain_fork_head_blk_id = cc.fork_db_pending_head_block_id();
//  dlog("updating chain info lib ${lib}, head ${head}, fork ${fork}",
//           ("lib", chain_lib_num)("head", chain_head_blk_num)("fork", chain_fork_head_blk_num) );
}

std::tuple<uint32_t, uint32_t, uint32_t, block_id_type, block_id_type, block_id_type>
net_plugin_impl::get_chain_info() const {
  std::lock_guard<std::mutex> g(chain_info_mtx);
  return std::make_tuple(
      chain_lib_num, chain_head_blk_num, chain_fork_head_blk_num,
      chain_lib_id, chain_head_blk_id, chain_fork_head_blk_id);
}

connection_ptr net_plugin_impl::find_connection(const string &host) const {
  for (const auto &c: connections)
    if (c->peer_address() == host) return c;
  return connection_ptr();
}

void net_plugin_impl::start_listen_loop() {
  connection_ptr new_connection = std::make_shared<connection>();
  new_connection->connecting = true;
  new_connection->strand.post([this, new_connection = std::move(new_connection)]() {
    acceptor->async_accept(*new_connection->socket,
                           boost::asio::bind_executor(new_connection->strand,
                                                      [new_connection, socket =
                                                      new_connection->socket, this](boost::system::error_code ec) {
                                                        if (!ec) {
                                                          uint32_t visitors = 0;
                                                          uint32_t from_addr = 0;
                                                          boost::system::error_code rec;
                                                          const auto
                                                              &paddr_add = socket->remote_endpoint(rec).address();
                                                          string paddr_str;
                                                          if (rec) {
                                                            elog("Error getting remote endpoint: ${m}",
                                                                 ("m", rec.message()));
                                                          } else {
                                                            paddr_str = paddr_add.to_string();
                                                            for_each_connection([&visitors, &from_addr, &paddr_str](auto &conn) {
                                                              if (conn->socket_is_open()) {
                                                                if (conn->peer_address().empty()) {
                                                                  ++visitors;
                                                                  std::lock_guard<std::mutex> g_conn(conn->conn_mtx);
                                                                  if (paddr_str == conn->remote_endpoint_ip) {
                                                                    ++from_addr;
                                                                  }
                                                                }
                                                              }
                                                              return true;
                                                            });
                                                            if (from_addr < max_nodes_per_host && (max_client_count == 0
                                                                || visitors < max_client_count)) {
                                                              ilog("Accepted new connection: " + paddr_str);
                                                              new_connection->set_heartbeat_timeout(heartbeat_timeout);
                                                              if (new_connection->start_session()) {
                                                                std::lock_guard<std::shared_mutex>
                                                                    g_unique(connections_mtx);
                                                                connections.insert(new_connection);
                                                              }

                                                            } else {
                                                              if (from_addr >= max_nodes_per_host) {
                                                                dlog(
                                                                    "Number of connections (${n}) from ${ra} exceeds limit ${l}",
                                                                    ("n", from_addr + 1)("ra", paddr_str)("l",
                                                                                                          max_nodes_per_host));
                                                              } else {
                                                                dlog("max_client_count ${m} exceeded",
                                                                     ("m", max_client_count));
                                                              }
                                                              // new_connection never added to connections and start_session not called, lifetime will end
                                                              boost::system::error_code ec;
                                                              socket->shutdown(tcp::socket::shutdown_both, ec);
                                                              socket->close(ec);
                                                            }
                                                          }
                                                        } else {
                                                          elog("Error accepting connection: ${m}", ("m", ec.message()));
                                                          // For the listed error codes below, recall start_listen_loop()
                                                          switch (ec.value()) {
                                                            case ECONNABORTED:
                                                            case EMFILE:
                                                            case ENFILE:
                                                            case ENOBUFS:
                                                            case ENOMEM:
                                                            case EPROTO:break;
                                                            default:return;
                                                          }
                                                        }
                                                        start_listen_loop();
                                                      }));
  });
}

void net_plugin_impl::ticker() {
  if (in_shutdown) return;
  std::lock_guard<std::mutex> g(keepalive_timer_mtx);
  keepalive_timer->expires_from_now(keepalive_interval);
  keepalive_timer->async_wait([my = shared_from_this()](boost::system::error_code ec) {
    my->ticker();
    if (ec) {
      if (my->in_shutdown) return;
      wlog("Peer keepalive ticked sooner than expected: ${m}", ("m", ec.message()));
    }

    tstamp current_time = connection::get_time();
    for_each_connection([current_time](auto &c) {
      if (c->socket_is_open()) {
        c->strand.post([c, current_time]() {
          c->check_heartbeat(current_time);
        });
      }
      return true;
    });
  });
}



//------------------------------------------------------------------------
// net_plugin
//------------------------------------------------------------------------
net_plugin::net_plugin()
    : my(new net_plugin_impl) {
  my_impl = my.get();
}

net_plugin::~net_plugin() {
}

void net_plugin::set_program_options(CLI::App &cli, CLI::App &config) {
  config.add_option("--p2p-listen-endpoint",
                    "The actual host:port used to listen for incoming p2p connections.")->default_str("0.0.0.0:9876");
  config.add_option("--p2p-peer-address", "The public endpoint of a peer node to connect to.")->take_all();
}

void net_plugin::plugin_initialize(const CLI::App &cli, const CLI::App &config) {
  ilog("Initialize net_plugin");
  try {
//    my->sync_master.reset( new sync_manager( options.at( "sync-fetch-span" ).as<uint32_t>()));

    my->connector_period = std::chrono::seconds(60); // number of seconds to wait before cleaning up dead connections
    my->max_cleanup_time_ms = 1000; // max connection cleanup time per cleanup call in millisec
    my->txn_exp_period = def_txn_expire_wait;
    my->resp_expected_period = def_resp_expected_wait;
    my->max_client_count = 5; // maximum number of clients from which connections are accepted, use 0 for no limit
    my->max_nodes_per_host = 1;
    my->p2p_accept_transactions = true;
    my->p2p_reject_incomplete_blocks = true;
    my->use_socket_read_watermark = false;
    my->keepalive_interval = std::chrono::milliseconds(30000);
    my->heartbeat_timeout = std::chrono::milliseconds(30000 * 2);

    my->p2p_address = "0.0.0.0:9876"; // The actual host:port used to listen for incoming p2p connections
    if (config.count("--p2p-listen-endpoint")) {
      my->p2p_address = config.get_option("--p2p-listen-endpoint")->as<string>();
    }

//    my->supplied_peers = {"127.0.0.1:9877", "127.0.0.1:9878"}; // The public endpoint of a peer node to connect to.
    if (config.count("--p2p-peer-address")) {
      my->supplied_peers = {config.get_option("--p2p-peer-address")->as<string>()};
    }

    //my->p2p_server_address = "0.0.0.0:9876"; // An externally accessible host:port for identifying this node. Defaults to p2p-listen-endpoint
    my->thread_pool_size = 2; // number of threads to use


    //my->user_agent_name = ""; // The name supplied to identify this node amongst the peers

    // Can be 'any' or 'producers' or 'specified' or 'none'. If 'specified', peer-key must be specified at least once. If only 'producers', peer-key is not required. 'producers' and 'specified' may be combined.
    //if (options.count("allowed-connection")) {
    my->allowed_connections = net_plugin_impl::Producers;

//    my->chain_plug = app().find_plugin<chain_plugin>();
//    my->chain_id = my->chain_plug->get_chain_id();
    fc::rand_pseudo_bytes(my->node_id.data(), my->node_id.data_size());
//    const controller &cc = my->chain_plug->chain();

//    if (my->p2p_accept_transactions) {
//      my->chain_plug->enable_accept_transactions();
//    }

  } FC_LOG_AND_RETHROW()
}

void net_plugin::plugin_startup() {
  ilog("Start net_plugin");
  try {
    ilog("my node_id is ${id}", ("id", my->node_id));

//    my->producer_plug = app().find_plugin<producer_plugin>();

    my->thread_pool.emplace("net", my->thread_pool_size);

//    my->dispatcher.reset( new dispatch_manager( my_impl->thread_pool->get_executor() ) );

    if (!my->p2p_accept_transactions && my->p2p_address.size()) {
      ilog("\n"
           "***********************************\n"
           "* p2p-accept-transactions = false *\n"
           "*    Transactions not forwarded   *\n"
           "***********************************\n");
    }

    tcp::endpoint listen_endpoint;
    if (my->p2p_address.size() > 0) {
      auto host = my->p2p_address.substr(0, my->p2p_address.find(':'));
      auto port = my->p2p_address.substr(host.size() + 1, my->p2p_address.size());
      tcp::resolver resolver(my->thread_pool->get_executor());
      // Note: need to add support for IPv6 too?
      listen_endpoint = *resolver.resolve(tcp::v4(), host, port);

      my->acceptor.reset(new tcp::acceptor(my_impl->thread_pool->get_executor()));

      if (!my->p2p_server_address.empty()) {
        my->p2p_address = my->p2p_server_address;
      } else {
        if (listen_endpoint.address().to_v4() == address_v4::any()) {
          boost::system::error_code ec;
          auto host = host_name(ec);
          if (ec.value() != boost::system::errc::success) {

            FC_THROW_EXCEPTION(fc::invalid_arg_exception,
                               "Unable to retrieve host_name. ${msg}", ("msg", ec.message()));

          }
          auto port = my->p2p_address.substr(my->p2p_address.find(':'), my->p2p_address.size());
          my->p2p_address = host + port;
        }
      }
    }

    if (my->acceptor) {
      try {
        my->acceptor->open(listen_endpoint.protocol());
        my->acceptor->set_option(tcp::acceptor::reuse_address(true));
        my->acceptor->bind(listen_endpoint);
        my->acceptor->listen();
      } catch (const std::exception &e) {
        elog("net_plugin::plugin_startup failed to bind to port ${port}", ("port", listen_endpoint.port()));
        throw e;
      }
      ilog("starting listener, max clients is ${mc}", ("mc", my->max_client_count));
      my->start_listen_loop();
    }
    {
//      chain::controller &cc = my->chain_plug->chain();
//      cc.accepted_block.connect([my = my](const block_state_ptr &s) {
//        my->on_accepted_block(s);
//      });
//      cc.pre_accepted_block.connect([my = my](const signed_block_ptr &s) {
//        my->on_pre_accepted_block(s);
//      });
//      cc.irreversible_block.connect([my = my](const block_state_ptr &s) {
//        my->on_irreversible_block(s);
//      });
    }

    {
      std::lock_guard<std::mutex> g(my->keepalive_timer_mtx);
      my->keepalive_timer.reset(new boost::asio::steady_timer(my->thread_pool->get_executor()));
    }
    my->ticker();

//    my->incoming_transaction_ack_subscription = app().get_channel<compat::channels::transaction_ack>().subscribe(
//        std::bind(&net_plugin_impl::transaction_ack, my.get(), std::placeholders::_1));

    my->start_monitors();

    my->update_chain_info();

    for (const auto &seed_node: my->supplied_peers) {
      connect(seed_node);
    }

  } catch (...) {
    // always want plugin_shutdown even on exception
    plugin_shutdown();
    throw;
  }
}

void net_plugin::plugin_shutdown() {
}

string net_plugin::connect(const string &host) {
  std::lock_guard<std::shared_mutex> g(my->connections_mtx);
  if (my->find_connection(host))
    return "already connected";

  connection_ptr c = std::make_shared<connection>(host);
  dlog("calling active connector: ${h}", ("h", host));
  if (c->resolve_and_connect()) {
    dlog("adding new connection to the list: ${c}", ("c", c->peer_name()));
    c->set_heartbeat_timeout(my->heartbeat_timeout);
    my->connections.insert(c);
  }
  return "added connection";
}

string net_plugin::disconnect(const string &host) {
  std::lock_guard<std::shared_mutex> g(my->connections_mtx);
  for (auto itr = my->connections.begin(); itr != my->connections.end(); ++itr) {
    if ((*itr)->peer_address() == host) {
      ilog("disconnecting: ${p}", ("p", (*itr)->peer_name()));
      (*itr)->close();
      my->connections.erase(itr);
      return "connection removed";
    }
  }
  return "no known connection for host";
}

std::optional<connection_status> net_plugin::status(const string &endpoint) const {
  return std::optional<connection_status>();
}

std::vector<connection_status> net_plugin::connections() const {
  vector<connection_status> result;
  std::shared_lock<std::shared_mutex> g(my->connections_mtx);
  result.reserve(my->connections.size());
  for (const auto &c: my->connections) {
    result.push_back(c->get_status());
  }
  return result;
}



//------------------------------------------------------------------------
// connection
//------------------------------------------------------------------------
connection::connection(string endpoint)
    : peer_addr(endpoint),
      strand(my_impl->thread_pool->get_executor()),
      socket(new tcp::socket(my_impl->thread_pool->get_executor())),
      connection_id(++my_impl->current_connection_id),
      response_expected_timer(my_impl->thread_pool->get_executor()),
      last_handshake_recv(),
      last_handshake_sent() {
  ilog("creating connection to ${n}", ("n", endpoint));
}

connection::connection()
    : peer_addr(),
      strand(my_impl->thread_pool->get_executor()),
      socket(new tcp::socket(my_impl->thread_pool->get_executor())),
      connection_id(++my_impl->current_connection_id),
      response_expected_timer(my_impl->thread_pool->get_executor()),
      last_handshake_recv(),
      last_handshake_sent() {
  dlog("new connection object created");
}

bool connection::resolve_and_connect() {
  switch (no_retry) {
    case no_reason:
    case wrong_version:
    case benign_other:break;
    default:dlog("Skipping connect due to go_away reason ${r}", ("r", reason_str(no_retry)));
      return false;
  }

  string::size_type colon = peer_address().find(':');
  if (colon == std::string::npos || colon == 0) {
    elog("Invalid peer address. must be \"host:port[:<blk>|<trx>]\": ${p}", ("p", peer_address()));
    return false;
  }

  connection_ptr c = shared_from_this();

  if (consecutive_immediate_connection_close > def_max_consecutive_immediate_connection_close
      || no_retry == benign_other) {
    auto connector_period_us = std::chrono::duration_cast<std::chrono::microseconds>(my_impl->connector_period);
    std::lock_guard<std::mutex> g(c->conn_mtx);
    if (last_close == fc::time_point()
        || last_close > fc::time_point::now() - fc::microseconds(connector_period_us.count())) {
      return true; // true so doesn't remove from valid connections
    }
  }

  strand.post([c]() {
    string::size_type colon = c->peer_address().find(':');
    string::size_type colon2 = c->peer_address().find(':', colon + 1);
    string host = c->peer_address().substr(0, colon);
    string port = c->peer_address().substr(colon + 1, colon2 == string::npos ? string::npos : colon2 - (colon + 1));
    idump((host)(port));
    c->set_connection_type(c->peer_address());

    auto resolver = std::make_shared<tcp::resolver>(my_impl->thread_pool->get_executor());
    connection_wptr weak_conn = c;
    // Note: need to add support for IPv6 too
    resolver->async_resolve(tcp::v4(), host, port,
                            boost::asio::bind_executor(c->strand,
                                                       [resolver, weak_conn](const boost::system::error_code &err,
                                                                             tcp::resolver::results_type endpoints) {
                                                         auto c = weak_conn.lock();
                                                         if (!c) return;
                                                         if (!err) {
                                                           c->connect(resolver, endpoints);
                                                         } else {
                                                           elog(
                                                               "Unable to resolve ${add}: ${error}",
                                                               ("add", c->peer_name())("error",
                                                                                       err.message()));
                                                           c->connecting = false;
                                                           ++c->consecutive_immediate_connection_close;
                                                         }
                                                       }));
  });
  return true;
}

connection_status connection::get_status() const {
  connection_status stat;
  stat.peer = peer_addr;
  stat.connecting = connecting;
  stat.syncing = syncing;
  std::lock_guard<std::mutex> g(conn_mtx);
  stat.last_handshake = last_handshake_recv;
  return stat;
}

bool connection::connected() {
  return socket_is_open() && !connecting;
}

void connection::set_connection_type(const string &peer_add) {
  // host:port:[<trx>|<blk>]
  string::size_type colon = peer_add.find(':');
  string::size_type colon2 = peer_add.find(':', colon + 1);
  string::size_type end = colon2 == string::npos
                          ? string::npos : peer_add.find_first_of(" :+=.,<>!$%^&(*)|-#@\t",
                                                                  colon2
                                                                      + 1); // future proof by including most symbols without using regex
  string host = peer_add.substr(0, colon);
  string port = peer_add.substr(colon + 1, colon2 == string::npos ? string::npos : colon2 - (colon + 1));
  string type = colon2 == string::npos ? "" : end == string::npos ?
                                              peer_add.substr(colon2 + 1) : peer_add.substr(colon2 + 1,
                                                                                            end - (colon2 + 1));

  if (type.empty()) {
    dlog("Setting connection type for: ${peer} to both transactions and blocks", ("peer", peer_add));
    connection_type = both;
  } else if (type == "trx") {
    dlog("Setting connection type for: ${peer} to transactions only", ("peer", peer_add));
    connection_type = transactions_only;
  } else if (type == "blk") {
    dlog("Setting connection type for: ${peer} to blocks only", ("peer", peer_add));
    connection_type = blocks_only;
  } else {
    wlog("Unknown connection type: ${t}", ("t", type));
  }
}

void connection::connect(const std::shared_ptr<tcp::resolver> &resolver, tcp::resolver::results_type endpoints) {
  switch (no_retry) {
    case no_reason:
    case wrong_version:
    case benign_other:break;
    default:return;
  }
  connecting = true;
  pending_message_buffer.reset();
  buffer_queue.clear_out_queue();
  boost::asio::async_connect(*socket, endpoints,
                             boost::asio::bind_executor(strand,
                                                        [resolver, c = shared_from_this(),
                                                            socket = socket](const boost::system::error_code &err,
                                                                             const tcp::endpoint &endpoint) {
                                                          if (!err && socket->is_open() && socket == c->socket) {
                                                            if (c->start_session()) {
                                                              c->send_handshake();
                                                            }
                                                          } else {
                                                            elog("connection failed to ${peer}: ${error}",
                                                                 ("peer", c->peer_name())("error", err.message()));
                                                            c->close(false);
                                                          }
                                                        }));
}

bool connection::start_session() {
//  verify_strand_in_this_thread( strand, __func__, __LINE__ );

  update_endpoints();
  boost::asio::ip::tcp::no_delay nodelay(true);
  boost::system::error_code ec;
  socket->set_option(nodelay, ec);
  if (ec) {
    elog("connection failed (set_option) ${peer}: ${e1}", ("peer", peer_name())("e1", ec.message()));
    close();
    return false;
  } else {
    dlog("connected to ${peer}", ("peer", peer_name()));
    socket_open = true;
    start_read_message();
    return true;
  }
}

void connection::send_handshake(bool force) {
  strand.post([force, c = shared_from_this()]() {
    std::unique_lock<std::mutex> g_conn(c->conn_mtx);
    if (c->populate_handshake(c->last_handshake_sent, force)) {
      static_assert(std::is_same_v<decltype(c->sent_handshake_count), int16_t>, "INT16_MAX based on int16_t");
      if (c->sent_handshake_count == INT16_MAX) c->sent_handshake_count = 1; // do not wrap
      c->last_handshake_sent.generation = ++c->sent_handshake_count;
      auto last_handshake_sent = c->last_handshake_sent;
      g_conn.unlock();
      ilog("Sending handshake generation ${g} to ${ep}, lib ${lib}, head ${head}, id ${id}",
           ("g", last_handshake_sent.generation)("ep", c->peer_name())
               ("lib", last_handshake_sent.last_irreversible_block_num)
               ("head", last_handshake_sent.head_num)("id", last_handshake_sent.head_id.str().substr(8, 16)));
      c->enqueue(last_handshake_sent);
    }
  });
}

void connection::close(bool reconnect, bool shutdown) {
  strand.post([self = shared_from_this(), reconnect, shutdown]() {
    connection::_close(self.get(), reconnect, shutdown);
  });
}

void connection::_close(connection *self, bool reconnect, bool shutdown) {
  self->socket_open = false;
  boost::system::error_code ec;
  if (self->socket->is_open()) {
    self->socket->shutdown(tcp::socket::shutdown_both, ec);
    self->socket->close(ec);
  }
  self->socket.reset(new tcp::socket(my_impl->thread_pool->get_executor()));
  self->flush_queues();
  self->connecting = false;
  self->syncing = false;
//  self->block_status_monitor_.reset();
  ++self->consecutive_immediate_connection_close;
  bool has_last_req = false;
  {
    std::lock_guard<std::mutex> g_conn(self->conn_mtx);
//    has_last_req = self->last_req.has_value();
    self->last_handshake_recv = handshake_message();
    self->last_handshake_sent = handshake_message();
    self->last_close = fc::time_point::now();
    self->conn_node_id = fc::sha256();
  }
  if (has_last_req && !shutdown) {
//    my_impl->dispatcher->retry_fetch( self->shared_from_this() );
  }
//  self->peer_requested.reset();
  self->sent_handshake_count = 0;
//  if( !shutdown) my_impl->sync_master->sync_reset_lib_num( self->shared_from_this() );
  ilog("closing '${a}', ${p}", ("a", self->peer_address())("p", self->peer_name()));
  dlog("canceling wait on ${p}", ("p", self->peer_name())); // peer_name(), do not hold conn_mtx
  self->cancel_wait();

  if (reconnect && !shutdown) {
    my_impl->start_conn_timer(std::chrono::milliseconds(100), connection_wptr());
  }
}

const string connection::peer_name() {
  std::lock_guard<std::mutex> g_conn(conn_mtx);
  if (!last_handshake_recv.p2p_address.empty()) {
    return last_handshake_recv.p2p_address;
  }
  if (!peer_address().empty()) {
    return peer_address();
  }
  if (remote_endpoint_port != unknown) {
    return remote_endpoint_ip + ":" + remote_endpoint_port;
  }
  return "connecting client";
}

void connection::update_endpoints() {
  boost::system::error_code ec;
  boost::system::error_code ec2;
  auto rep = socket->remote_endpoint(ec);
  auto lep = socket->local_endpoint(ec2);
  std::lock_guard<std::mutex> g_conn(conn_mtx);
  remote_endpoint_ip = ec ? unknown : rep.address().to_string();
  remote_endpoint_port = ec ? unknown : std::to_string(rep.port());
  local_endpoint_ip = ec2 ? unknown : lep.address().to_string();
  local_endpoint_port = ec2 ? unknown : std::to_string(lep.port());
}

bool connection::populate_handshake(handshake_message &hello, bool force) {
  namespace sc = std::chrono;
  bool send = force;
//  hello.network_version = net_version_base + net_version;
  const auto prev_head_id = hello.head_id;
  uint32_t lib, head;
  std::tie(lib, std::ignore, head,
           hello.last_irreversible_block_id, std::ignore, hello.head_id) = my_impl->get_chain_info();
  // only send handshake if state has changed since last handshake
  send |= lib != hello.last_irreversible_block_num;
  send |= head != hello.head_num;
  send |= prev_head_id != hello.head_id;
//  if (!send) return false;
  hello.last_irreversible_block_num = lib;
  hello.head_num = head;
//  hello.chain_id = my_impl->chain_id;
  hello.node_id = my_impl->node_id;
//  hello.key = my_impl->get_authentication_key();
  hello.time = sc::duration_cast<sc::nanoseconds>(sc::system_clock::now().time_since_epoch()).count();
  hello.token = fc::sha256::hash(hello.time);
//  hello.sig = my_impl->sign_compact(hello.key, hello.token);
  // If we couldn't sign, don't send a token.
//  if(hello.sig == chain::signature_type())
//    hello.token = sha256();
  hello.p2p_address = my_impl->p2p_address;
  if (is_transactions_only_connection()) hello.p2p_address += ":trx";
  if (is_blocks_only_connection()) hello.p2p_address += ":blk";
  hello.p2p_address += " - " + hello.node_id.str().substr(0, 7);
//#if defined( __APPLE__ )
//  hello.os = "osx";
//#elif defined( __linux__ )
//  hello.os = "linux";
//#elif defined( _WIN32 )
//      hello.os = "win32";
//#else
//      hello.os = "other";
//#endif
//  hello.agent = my_impl->user_agent_name;

  return true;
}

void connection::start_read_message() {
  try {
    std::size_t minimum_read =
        std::atomic_exchange<decltype(outstanding_read_bytes.load())>(&outstanding_read_bytes, 0);
    minimum_read = minimum_read != 0 ? minimum_read : message_header_size;

    if (my_impl->use_socket_read_watermark) {
      const size_t max_socket_read_watermark = 4096;
      std::size_t socket_read_watermark = std::min<std::size_t>(minimum_read, max_socket_read_watermark);
      boost::asio::socket_base::receive_low_watermark read_watermark_opt(socket_read_watermark);
      boost::system::error_code ec;
      socket->set_option(read_watermark_opt, ec);
      if (ec) {
        elog("unable to set read watermark ${peer}: ${e1}", ("peer", peer_name())("e1", ec.message()));
      }
    }

    auto completion_handler =
        [minimum_read](boost::system::error_code ec, std::size_t bytes_transferred) -> std::size_t {
          if (ec || bytes_transferred >= minimum_read) {
            return 0;
          } else {
            return minimum_read - bytes_transferred;
          }
        };

    uint32_t write_queue_size = buffer_queue.write_queue_size();
    if (write_queue_size > def_max_write_queue_size) {
      elog("write queue full ${s} bytes, giving up on connection, closing connection to: ${p}",
           ("s", write_queue_size)("p", peer_name()));
      close(false);
      return;
    }

    boost::asio::async_read(*socket,
                            pending_message_buffer.get_buffer_sequence_for_boost_async_read(), completion_handler,
                            boost::asio::bind_executor(strand,
                                                       [conn = shared_from_this(),
                                                           socket = socket](boost::system::error_code ec,
                                                                            std::size_t bytes_transferred) {
                                                         // may have closed connection and cleared pending_message_buffer
                                                         if (!conn->socket_is_open() || socket != conn->socket) return;

                                                         bool close_connection = false;
                                                         try {
                                                           if (!ec) {
                                                             if (bytes_transferred
                                                                 > conn->pending_message_buffer.bytes_to_write()) {
                                                               elog(
                                                                   "async_read_some callback: bytes_transfered = ${bt}, buffer.bytes_to_write = ${btw}",
                                                                   ("bt", bytes_transferred)("btw",
                                                                                             conn->pending_message_buffer.bytes_to_write()));
                                                             }
//                                                               EOS_ASSERT(bytes_transferred <= conn->pending_message_buffer.bytes_to_write(), plugin_exception, "");
                                                             conn->pending_message_buffer.advance_write_ptr(
                                                                 bytes_transferred);
                                                             while (conn->pending_message_buffer.bytes_to_read() > 0) {
                                                               uint32_t bytes_in_buffer =
                                                                   conn->pending_message_buffer.bytes_to_read();

                                                               if (bytes_in_buffer < message_header_size) {
                                                                 conn->outstanding_read_bytes =
                                                                     message_header_size - bytes_in_buffer;
                                                                 break;
                                                               } else {
                                                                 uint32_t message_length;
                                                                 auto index = conn->pending_message_buffer.read_index();
                                                                 conn->pending_message_buffer.peek(&message_length,
                                                                                                   sizeof(message_length),
                                                                                                   index);
                                                                 if (message_length > def_send_buffer_size * 2
                                                                     || message_length == 0) {
                                                                   elog("incoming message length unexpected (${i})",
                                                                        ("i", message_length));
                                                                   close_connection = true;
                                                                   break;
                                                                 }

                                                                 auto total_message_bytes =
                                                                     message_length + message_header_size;

                                                                 if (bytes_in_buffer >= total_message_bytes) {
                                                                   conn->pending_message_buffer.advance_read_ptr(
                                                                       message_header_size);
                                                                   conn->consecutive_immediate_connection_close = 0;
                                                                   if (!conn->process_next_message(message_length)) {
                                                                     return;
                                                                   }
                                                                 } else {
                                                                   auto outstanding_message_bytes =
                                                                       total_message_bytes - bytes_in_buffer;
                                                                   auto available_buffer_bytes =
                                                                       conn->pending_message_buffer.bytes_to_write();
                                                                   if (outstanding_message_bytes
                                                                       > available_buffer_bytes) {
                                                                     conn->pending_message_buffer.add_space(
                                                                         outstanding_message_bytes
                                                                             - available_buffer_bytes);
                                                                   }

                                                                   conn->outstanding_read_bytes =
                                                                       outstanding_message_bytes;
                                                                   break;
                                                                 }
                                                               }
                                                             }
                                                             if (!close_connection) conn->start_read_message();
                                                           } else {
                                                             if (ec.value() != boost::asio::error::eof) {
                                                               elog("Error reading message: ${m}", ("m", ec.message()));
                                                             } else {
                                                               ilog("Peer closed connection");
                                                             }
                                                             close_connection = true;
                                                           }
                                                         }
                                                         catch (const std::bad_alloc &) {
                                                           throw;
                                                         }
                                                         catch (const boost::interprocess::bad_alloc &) {
                                                           throw;
                                                         }
                                                         catch (const fc::exception &ex) {
                                                           elog("Exception in handling read data ${s}",
                                                                ("s", ex.to_string()));
                                                           close_connection = true;
                                                         }
                                                         catch (const std::exception &ex) {
                                                           elog("Exception in handling read data: ${s}",
                                                                ("s", ex.what()));
                                                           close_connection = true;
                                                         }
                                                         catch (...) {
                                                           elog("Undefined exception handling read data");
                                                           close_connection = true;
                                                         }

                                                         if (close_connection) {
                                                           elog("Closing connection to: ${p}",
                                                                ("p", conn->peer_name()));
                                                           conn->close();
                                                         }
                                                       }));
  } catch (...) {
    elog("Undefined exception in start_read_message, closing connection to: ${p}", ("p", peer_name()));
    close();
  }
}

bool connection::process_next_message(uint32_t message_length) {
  try {
    latest_msg_time = get_time();

    // if next message is a block we already have, exit early
    auto peek_ds = pending_message_buffer.create_peek_datastream();
//    unsigned_int which{};
//    fc::raw::unpack(peek_ds, which);
//    if( which == signed_block_which || which == signed_block_v0_which ) {
//      return process_next_block_message( message_length );
//
//    } else if( which == trx_message_v1_which || which == packed_transaction_v0_which ) {
//      return process_next_trx_message( message_length );
//
//    } else {
    auto ds = pending_message_buffer.create_datastream();
    net_message msg;
    fc::raw::unpack(ds, msg);
    msg_handler m(shared_from_this());
    std::visit(m, msg);
//    }

  } catch (const fc::exception &e) {
    elog("Exception in handling message from ${p}: ${s}",
         ("p", peer_name())("s", e.to_detail_string()));
    close();
    return false;
  }
  return true;
}

void connection::cancel_wait() {
  std::lock_guard<std::mutex> g(response_expected_timer_mtx);
  response_expected_timer.cancel();
}

void connection::flush_queues() {
  buffer_queue.clear_write_queue();
}

void connection::enqueue(const net_message &m) {
//  verify_strand_in_this_thread( strand, __func__, __LINE__ );
  go_away_reason close_after_send = no_reason;
  if (std::holds_alternative<go_away_message>(m)) {
    close_after_send = std::get<go_away_message>(m).reason;
  }

  buffer_factory buff_factory;
  auto send_buffer = buff_factory.get_send_buffer(m);
  enqueue_buffer(send_buffer, close_after_send);
}

void connection::enqueue_buffer(const std::shared_ptr<std::vector<char>> &send_buffer,
                                go_away_reason close_after_send,
                                bool to_sync_queue) {
  connection_ptr self = shared_from_this();
  queue_write(send_buffer,
              [conn{std::move(self)}, close_after_send](boost::system::error_code ec, std::size_t) {
                if (ec) return;
                if (close_after_send != no_reason) {
                  ilog("sent a go away message: ${r}, closing connection to ${p}",
                       ("r", reason_str(close_after_send))("p", conn->peer_name()));
                  conn->close();
                  return;
                }
              },
              to_sync_queue);
}

void connection::queue_write(const std::shared_ptr<vector<char>> &buff,
                             std::function<void(boost::system::error_code, std::size_t)> callback,
                             bool to_sync_queue) {
  if (!buffer_queue.add_write_queue(buff, callback, to_sync_queue)) {
    wlog("write_queue full ${s} bytes, giving up on connection ${p}",
         ("s", buffer_queue.write_queue_size())("p", peer_name()));
    close();
    return;
  }
  do_queue_write();
}

void connection::do_queue_write() {
  if (!buffer_queue.ready_to_send())
    return;
  connection_ptr c(shared_from_this());

  std::vector<boost::asio::const_buffer> bufs;
  buffer_queue.fill_out_buffer(bufs);

  strand.post([c{std::move(c)}, bufs{std::move(bufs)}]() {
    boost::asio::async_write(*c->socket, bufs,
                             boost::asio::bind_executor(c->strand,
                                                        [c, socket = c->socket](boost::system::error_code ec,
                                                                                std::size_t w) {
                                                          try {
                                                            c->buffer_queue.clear_out_queue();
                                                            // May have closed connection and cleared buffer_queue
                                                            if (!c->socket_is_open() || socket != c->socket) {
                                                              ilog("async write socket ${r} before callback: ${p}",
                                                                   ("r", c->socket_is_open() ? "changed"
                                                                                             : "closed")("p",
                                                                                                         c->peer_name()));
                                                              c->close();
                                                              return;
                                                            }

                                                            if (ec) {
                                                              if (ec.value() != boost::asio::error::eof) {
                                                                elog("Error sending to peer ${p}: ${i}",
                                                                     ("p", c->peer_name())("i", ec.message()));
                                                              } else {
                                                                wlog("connection closure detected on write to ${p}",
                                                                     ("p", c->peer_name()));
                                                              }
                                                              c->close();
                                                              return;
                                                            }

                                                            c->buffer_queue.out_callback(ec, w);

                                                            c->enqueue_sync_block();
                                                            c->do_queue_write();
                                                          } catch (const std::bad_alloc &) {
                                                            throw;
                                                          } catch (const boost::interprocess::bad_alloc &) {
                                                            throw;
                                                          } catch (const fc::exception &ex) {
                                                            elog("Exception in do_queue_write to ${p} ${s}",
                                                                 ("p", c->peer_name())("s", ex.to_string()));
                                                          } catch (const std::exception &ex) {
                                                            elog("Exception in do_queue_write to ${p} ${s}",
                                                                 ("p", c->peer_name())("s", ex.what()));
                                                          } catch (...) {
                                                            elog("Exception in do_queue_write to ${p}",
                                                                 ("p", c->peer_name()));
                                                          }
                                                        }));
  });
}

bool connection::enqueue_sync_block() {
//  if( !peer_requested ) {
//    return false;
//  } else {
//    dlog("enqueue sync block ${num}", ("num", peer_requested->last + 1) );
//  }
//  uint32_t num = ++peer_requested->last;
//  if(num == peer_requested->end_block) {
//    peer_requested.reset();
//    ilog("completing enqueue_sync_block ${num} to ${p}", ("num", num)("p", peer_name()) );
//  }
//  connection_wptr weak = shared_from_this();
//  app().post( priority::medium, [num, weak{std::move(weak)}]() {
//    connection_ptr c = weak.lock();
//    if( !c ) return;
//    controller& cc = my_impl->chain_plug->chain();
//    signed_block_ptr sb;
//    try {
//      sb = cc.fetch_block_by_number( num );
//    } FC_LOG_AND_DROP();
//    if( sb ) {
//      c->strand.post( [c, sb{std::move(sb)}]() {
//        c->enqueue_block( sb, true );
//      });
//    } else {
//      c->strand.post( [c, num]() {
//        ilog("enqueue sync, unable to fetch block ${num}", ("num", num) );
//        c->send_handshake();
//      });
//    }
//  });

  return true;
}

void connection::check_heartbeat(tstamp current_time) {
  if (protocol_version >= heartbeat_interval) {
    if (latest_msg_time > 0 && current_time > latest_msg_time + hb_timeout) {
      no_retry = benign_other;
      if (!peer_address().empty()) {
        wlog("heartbeat timed out for peer address ${adr}", ("adr", peer_address()));
        close(true);  // reconnect
      } else {
        {
          std::lock_guard<std::mutex> g_conn(conn_mtx);
          wlog("heartbeat timed out from ${p} ", ("p", last_handshake_recv.p2p_address));
        }
        close(false); // don't reconnect
      }
      return;
    }
  }
  send_time();
}

void connection::send_time() {
  time_message xpkt;
  xpkt.org = rec;
  xpkt.rec = dst;
  xpkt.xmt = get_time();
  org = xpkt.xmt;
  enqueue(xpkt);
}

void connection::send_time(const time_message &msg) {
  time_message xpkt;
  xpkt.org = msg.xmt;
  xpkt.rec = msg.dst;
  xpkt.xmt = get_time();
  enqueue(xpkt);
}

bool connection::is_valid(const handshake_message &msg) {
  bool valid = true;
//  if (msg.last_irreversible_block_num > msg.head_num) {
//    wlog("Handshake message validation: last irreversible block (${i}) is greater than head block (${h})",
//             ("i", msg.last_irreversible_block_num)("h", msg.head_num) );
//    valid = false;
//  }
  if (msg.p2p_address.empty()) {
    wlog("Handshake message validation: p2p_address is null string");
    valid = false;
  }
  return valid;
}

void connection::handle_message(const handshake_message &msg) {
  dlog("received handshake_message");
  if (!is_valid(msg)) {
    elog("bad handshake message");
    enqueue(go_away_message(fatal_other));
    return;
  }
  dlog("received handshake gen ${g} from ${ep}, lib ${lib}, head ${head}",
       ("g", msg.generation)("ep", peer_name())
           ("lib", msg.last_irreversible_block_num)("head", msg.head_num));

  connecting = false;
  if (msg.generation == 1) {
    if (msg.node_id == my_impl->node_id) {
      elog("Self connection detected node_id ${id}. Closing connection", ("id", msg.node_id));
      enqueue(go_away_message(self));
      return;
    }

    if (peer_address().empty()) {
      set_connection_type(msg.p2p_address);
    }

    std::unique_lock<std::mutex> g_conn(conn_mtx);
    if (peer_address().empty() || last_handshake_recv.node_id == fc::sha256()) {
      g_conn.unlock();
      dlog("checking for duplicate");
      std::shared_lock<std::shared_mutex> g_cnts(my_impl->connections_mtx);
      for (const auto &check: my_impl->connections) {
        if (check.get() == this)
          continue;
        if (check->connected() && check->peer_name() == msg.p2p_address) {
          if (my_impl->p2p_address < msg.p2p_address) {
            // only the connection from lower p2p_address to higher p2p_address will be considered as a duplicate,
            // so there is no chance for both connections to be closed
            continue;
          }

          g_cnts.unlock();
          dlog("sending go_away duplicate to ${ep}", ("ep", msg.p2p_address));
          go_away_message gam(duplicate);
          g_conn.lock();
          gam.node_id = conn_node_id;
          g_conn.unlock();
          enqueue(gam);
          no_retry = duplicate;
          return;
        }
      }
    } else {
      dlog("skipping duplicate check, addr == ${pa}, id = ${ni}",
           ("pa", peer_address())("ni", last_handshake_recv.node_id));
      g_conn.unlock();
    }

    g_conn.lock();
    if (conn_node_id != msg.node_id) {
      conn_node_id = msg.node_id;
    }
    g_conn.unlock();

//    if( !my_impl->authenticate_peer( msg ) ) {
//      elog("Peer not authenticated.  Closing connection." );
//      enqueue( go_away_message( authentication ) );
//      return;
//    }

    if (sent_handshake_count == 0) {
      send_handshake();
    }
  }

  std::unique_lock<std::mutex> g_conn(conn_mtx);
  last_handshake_recv = msg;
  g_conn.unlock();
//  my_impl->sync_master->recv_handshake( shared_from_this(), msg );
}

void connection::handle_message(const go_away_message &msg) {
  wlog("received go_away_message, reason = ${r}", ("r", reason_str(msg.reason)));

  bool retry = no_retry == no_reason; // if no previous go away message
  no_retry = msg.reason;
  if (msg.reason == duplicate) {
    std::lock_guard<std::mutex> g_conn(conn_mtx);
    conn_node_id = msg.node_id;
  }
  if (msg.reason == wrong_version) {
    if (!retry) no_retry = fatal_other; // only retry once on wrong version
  } else {
    retry = false;
  }
  flush_queues();

  close(retry); // reconnect if wrong_version
}

void connection::handle_message(const time_message &msg) {
  dlog("received time_message");

  /* We've already lost however many microseconds it took to dispatch
   * the message, but it can't be helped.
   */
  msg.dst = get_time();

  // If the transmit timestamp is zero, the peer is horribly broken.
  if (msg.xmt == 0)
    return;                 /* invalid timestamp */

  if (msg.xmt == xmt)
    return;                 /* duplicate packet */

  xmt = msg.xmt;
  rec = msg.rec;
  dst = msg.dst;

  if (msg.org == 0) {
    send_time(msg);
    return;  // We don't have enough data to perform the calculation yet.
  }

  double offset = (double(rec - org) + double(msg.xmt - dst)) / 2;
  double NsecPerUsec{1000};

  org = 0;
  rec = 0;

  std::unique_lock<std::mutex> g_conn(conn_mtx);
  if (last_handshake_recv.generation == 0) {
    g_conn.unlock();
    send_handshake();
  }
}

} // namespace noir::net
