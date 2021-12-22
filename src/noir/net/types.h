#include <cinttypes>
#include <chrono>
#include <vector>

namespace noir::net {

using tstamp = std::chrono::system_clock::duration::rep;
using bytes = std::vector<char>;
using signature = bytes;

}
