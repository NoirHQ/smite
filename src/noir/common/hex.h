#pragma once
#include <vector>

namespace noir {

std::string to_hex(const char* d, uint32_t s);
std::string to_hex(const std::vector<char>& data);

uint8_t from_hex(char c);
size_t from_hex(const std::string& s, char* out, size_t outlen);
std::vector<char> from_hex(const std::string& s);
std::vector<char> from_hex(std::string&& s);

} // namespace noir
