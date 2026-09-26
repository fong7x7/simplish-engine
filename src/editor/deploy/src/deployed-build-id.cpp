#include "deployed-build-id.h"

#include "simplish-revision.h"

namespace eng::editor {

uint64_t deployedBuildId() {
  uint64_t hash = 0xCBF29CE484222325ULL;  // FNV-1a, 64-bit
  for (const char byte : SIMPLISH_REVISION) {
    hash = (hash ^ static_cast<uint8_t>(byte)) * 0x100000001B3ULL;
  }
  return hash;
}

}  // namespace eng::editor
