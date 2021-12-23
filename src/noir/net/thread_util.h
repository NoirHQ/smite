#pragma once

#include <fc/log/logger_config.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#include <future>
#include <memory>
#include <optional>

namespace noir::net {

/**
 * Wrapper class for boost asio thread pool and io_context run.
 * Also names threads so that tools like htop can see thread name.
 */
class named_thread_pool {
 public:
  // name_prefix is name appended with -## of thread.
  // short name_prefix (6 chars or under) is recommended as console_appender uses 9 chars for thread name
  named_thread_pool(std::string name_prefix, size_t num_threads)
      : _thread_pool(num_threads), _ioc(num_threads) {
    _ioc_work.emplace(boost::asio::make_work_guard(_ioc));
    for (size_t i = 0; i < num_threads; ++i) {
      boost::asio::post(_thread_pool, [&ioc = _ioc, name_prefix, i]() {
        std::string tn = name_prefix + "-" + std::to_string(i);
        fc::set_os_thread_name(tn);
        ioc.run();
      });
    }
  }

  // calls stop()
  ~named_thread_pool() {
    stop();
  }

  boost::asio::io_context &get_executor() { return _ioc; }

  // destroy work guard, stop io_context, join thread_pool, and stop thread_pool
  void stop() {
    _ioc_work.reset();
    _ioc.stop();
    _thread_pool.join();
    _thread_pool.stop();
  }

 private:
  using ioc_work_t = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

  boost::asio::thread_pool _thread_pool;
  boost::asio::io_context _ioc;
  std::optional<ioc_work_t> _ioc_work;
};

}
