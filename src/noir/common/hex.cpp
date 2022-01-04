#include <noir/common/check.h>
#include <noir/common/hex.h>

namespace noir {

std::string to_hex(std::span<const char> s) {
  std::string r;
  const char* to_hex = "0123456789abcdef";
  auto c = std::span((const uint8_t*)s.data(), s.size());
  for (const auto& val : c) {
    r += to_hex[val >> 4];
    r += to_hex[val & 0x0f];
  }
  return r;
}

uint8_t from_hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  check(false, "invalid hex character `" + std::string(1, c) + "`");
  return 0;
}

std::vector<char> from_hex(const std::string& s) {
  auto has_prefix = s.starts_with("0x");
  auto require_pad = s.size() % 2;
  auto size = (s.size() / 2) + require_pad - has_prefix;
  std::vector<char> result(size);
  auto c = s.begin() + has_prefix * 2;
  auto r = result.begin();
  for (; c != s.end() && r != result.end(); ++c, ++r) {
    if (!require_pad)
      *r = from_hex(*c++) << 4;
    *r |= from_hex(*c);
    require_pad = false;
  }
  return result;
}

std::vector<char> from_hex(std::string&& s) {
  return from_hex(s);
}

} // namespace noir
