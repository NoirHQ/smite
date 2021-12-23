#include <noir/net/net_plugin.h>
#include <noir/net/consensus/block.h>
#include <noir/net/consensus/tx.h>
#include <noir/net/queued_buffer.h>
#include <noir/net/thread_util.h>

#include <appbase/application.hpp>
#include <fc/network/message_buffer.hpp>
#include <fc/log/logger_config.hpp>

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

class connection;

using connection_ptr = std::shared_ptr<connection>;
using connection_wptr = std::weak_ptr<connection>;


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
  std::set<connection_ptr>
      connections;     // todo: switch to a thread safe container to avoid big mutex over complete collection

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

//------------------------------------------------------------------------
// net_plugin
//------------------------------------------------------------------------
net_plugin::net_plugin()
    : my(new net_plugin_impl) {
  my_impl = my.get();
}

net_plugin::~net_plugin() {
}

void net_plugin::plugin_initialize(const CLI::App &cli, const CLI::App &config) {
  ilog("Initialize net_plugin");
}

void net_plugin::plugin_startup() {
  ilog("Start net_plugin");
  try {
//    my->sync_master.reset( new sync_manager( options.at( "sync-fetch-span" ).as<uint32_t>()));

    my->connector_period = std::chrono::seconds(30); // number of seconds to wait before cleaning up dead connections
    my->max_cleanup_time_ms = 1000; // max connection cleanup time per cleanup call in millisec
    my->txn_exp_period = def_txn_expire_wait;
    my->resp_expected_period = def_resp_expected_wait;
    my->max_client_count = 5; // maximum number of clients from which connections are accepted, use 0 for no limit
    my->max_nodes_per_host = 1;
    my->p2p_accept_transactions = true;
    my->p2p_reject_incomplete_blocks = true;
    my->use_socket_read_watermark = false;
    my->keepalive_interval = std::chrono::milliseconds(1000);
    my->heartbeat_timeout = std::chrono::milliseconds(1000 * 2);

    my->p2p_address = "0.0.0.0:9876"; // The actual host:port used to listen for incoming p2p connections
    //my->p2p_server_address = "0.0.0.0:9876"; // An externally accessible host:port for identifying this node. Defaults to p2p-listen-endpoint
    my->thread_pool_size = 2;

    my->supplied_peers = {"127.0.0.1:9877", "127.0.0.1:9878"}; // The public endpoint of a peer node to connect to.

    //my->user_agent_name = ""; // The name supplied to identify this node amongst the peers

    // Can be 'any' or 'producers' or 'specified' or 'none'. If 'specified', peer-key must be specified at least once. If only 'producers', peer-key is not required. 'producers' and 'specified' may be combined.
    //if (options.count("allowed-connection")) {
    my->allowed_connections = net_plugin_impl::Producers;

//    my->chain_plug = app().find_plugin<chain_plugin>();
//    my->chain_id = my->chain_plug->get_chain_id();
//    fc::rand_pseudo_bytes(my->node_id.data(), my->node_id.data_size());
//    const controller &cc = my->chain_plug->chain();

//    if (my->p2p_accept_transactions) {
//      my->chain_plug->enable_accept_transactions();
//    }

  } FC_LOG_AND_RETHROW()
}

void net_plugin::plugin_shutdown() {
}

string net_plugin::connect(const string &endpoint) {
  return fc::string();
}

string net_plugin::disconnect(const string &endpoint) {
  return fc::string();
}

std::optional<connection_status> net_plugin::status(const string &endpoint) const {
  return std::optional<connection_status>();
}

std::vector<connection_status> net_plugin::connections() const {
  return std::vector<connection_status>();
}

//------------------------------------------------------------------------
// connection
//------------------------------------------------------------------------
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
//  void send_time(const time_message &msg);
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
//  void enqueue_buffer(const std::shared_ptr<std::vector<char>> &send_buffer,
//                      go_away_reason close_after_send,
//                      bool to_sync_queue = false);
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
//  void handle_message(const go_away_message &msg);
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
//  void handle_message(const time_message &msg);
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

} // namespace noir::net
