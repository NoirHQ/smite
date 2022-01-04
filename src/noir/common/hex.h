#pragma once
#include <span>
#include <vector>

namespace noir {

std::string to_hex(std::span<const char> s);

std::vector<char> from_hex(const std::string& s);
std::vector<char> from_hex(std::string&& s);

} // namespace noir
