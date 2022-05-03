#pragma once
#include <boost/asio/experimental/concurrent_channel.hpp>

namespace noir {

template<typename... Ts>
using Chan = boost::asio::experimental::concurrent_channel<void(boost::system::error_code, Ts...)>;

} // namespace noir
