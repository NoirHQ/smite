#pragma once
#include <string>

class str_const {// constexpr string
private:
  const char* const p_;
  const std::size_t sz_;

public:
  template<std::size_t N>
  constexpr explicit str_const(const char (&a)[N]) : p_(a), sz_(N - 1) {}
  constexpr char operator[](std::size_t n) {
    return n < sz_ ? p_[n] : throw std::out_of_range("");
  }
  constexpr std::size_t size() const {
    return sz_;
  }
  operator std::string() {
    return {p_, sz_};
  }
};
