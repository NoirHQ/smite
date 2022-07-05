#include <tendermint/abci/client/socket_client.h>
#include <noir/net/tcp_conn.h>

using namespace noir;

int main() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  auto cli = std::make_shared<abci::SocketClient<net::TcpConn>>("127.0.0.1:26658", true);
  cli->start();

  eo::runtime::execution_context.join();

  return 0;
}
