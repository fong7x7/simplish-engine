#include <engine/net/net-password.h>

namespace eng::net {

uint64_t netPasswordDigest(std::string_view password) {
  if (password.empty()) {
    return 0;
  }
  // FNV-1a, 64-bit; a password digesting to 0 would read as none, so it
  // is nudged off it.
  uint64_t hash = 0xCBF29CE484222325ULL;
  for (const char byte : password) {
    hash = (hash ^ static_cast<uint8_t>(byte)) * 0x100000001B3ULL;
  }
  return hash == 0 ? 1 : hash;
}

}  // namespace eng::net
