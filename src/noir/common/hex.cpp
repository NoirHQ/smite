#include <noir/common/check.h>
#include <noir/common/hex.h>

namespace noir {

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

  std::string to_hex(const char* d, uint32_t s) {
    std::string r;
    const char* to_hex = "0123456789abcdef";
    auto c = reinterpret_cast<const uint8_t*>(d);
    for (auto i = 0; i < s; ++i)
      (r += to_hex[(c[i]>>4)]) += to_hex[(c[i]&0x0f)];
    return r;
  }

  std::string to_hex(const std::vector<char>& data) {
    if (data.size())
      return to_hex(data.data(), data.size());
    return "";
  }

  size_t from_hex(const std::string& s, char* out, size_t outlen) {
    bool require_pad = s.size() % 2;
    auto out_pos = reinterpret_cast<uint8_t*>(out);
    auto out_end = out_pos + outlen;
    auto c = s.begin() + (s.find("0x") != std::string::npos) * 2;
    for (; c != s.end() && out_pos != out_end; ++c) {
      if (!require_pad)
        *out_pos = from_hex(*c++) << 4;
      *out_pos |= from_hex(*c);
      ++out_pos;
      require_pad = false;
    }
    return out_pos - reinterpret_cast<uint8_t*>(out);
  }

  std::vector<char> from_hex(const std::string& s) {
    auto has_prefix = s.find("0x") != std::string::npos;
    auto size = (s.size() / 2) + (s.size() % 2) - has_prefix;
    std::vector<char> tmp(size);
    from_hex(s, tmp.data(), tmp.size());
    return tmp;
  }

  std::vector<char> from_hex(std::string&& s) {
    return from_hex(s);
  }

} // namespace noir
