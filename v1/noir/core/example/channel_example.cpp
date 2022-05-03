#include <noir/core/channel.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <iostream>


using namespace noir;
using namespace boost::asio::experimental::awaitable_operators;
using boost::asio::experimental::as_tuple;

struct Done {};

boost::asio::awaitable<void> recv(Chan<Done>& done, Chan<int>& value) {
  for (;;) {
    auto res =
      co_await(done.async_receive(as_tuple(boost::asio::use_awaitable)) || value.async_receive(boost::asio::use_awaitable));
    switch (res.index()) {
    case 0:
      co_return;
    case 1:
      std::cout << std::get<1>(res) << std::endl;
    }
  }
}

boost::asio::awaitable<void> send(Chan<Done>& done, Chan<int>& value, boost::asio::steady_timer&& timer) {
  boost::system::error_code ec;
  for (auto i = 1; i <= 5; ++i) {
    co_await value.async_send(ec, i, boost::asio::use_awaitable);
    timer.expires_after(std::chrono::seconds(1));
    co_await timer.async_wait(boost::asio::use_awaitable);
  }
  done.close();
  //co_await done.async_send(ec, {}, boost::asio::use_awaitable);
}

int main() {
  boost::asio::io_context io_context{};

  Chan<Done> done{io_context};
  Chan<int> value{io_context};

  boost::asio::co_spawn(io_context, recv(done, value), boost::asio::detached);
  boost::asio::co_spawn(io_context, send(done, value, boost::asio::steady_timer{io_context}), boost::asio::detached);

  io_context.run();
}
