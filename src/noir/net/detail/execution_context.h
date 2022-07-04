#pragma once

namespace noir::net {

template<typename T>
concept ExecutionContext = requires(T v) {
  typename T::executor_type;
  v.get_executor();
};

} // namespace noir::net
