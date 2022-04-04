#include <tendermint/abci/client/socket_client.h>
#include <tendermint/common/thread_utils.h>
#include <tendermint/net/tcp_conn.h>
#include <boost/asio/io_context.hpp>

using namespace tendermint;

int main() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  named_thread_pool pool("main", 4);

  auto cli = std::make_shared<abci::SocketClient<net::TCPConn>>("127.0.0.1:26658", true, pool.get_executor());
  cli->start();

  pool.get_executor().run();

  return 0;
}
