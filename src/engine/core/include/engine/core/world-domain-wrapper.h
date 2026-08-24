#pragma once

#include "plugin-api.h"
#include "vx-flag-binary.h"

#include <cstdint>
#include <string_view>

namespace eng {

/// Type-safe C++ wrapper around the C ABI world domain function table.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class WorldDomainWrapper {
public:
  explicit WorldDomainWrapper(const PluginAPI::WorldDomain& api) : api_(&api) {}

  float getTimeMs() const { return api_->get_time_ms(); }

  bool isChunkLoaded(int64_t cx, int64_t cy, int64_t cz) const {
    return api_->is_chunk_loaded(cx, cy, cz);
  }

  int32_t getFlag(std::string_view key) const {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    return api_->get_flag(key.data());
  }

  bool setFlag(std::string_view key, VxFlagBinary value) {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    return api_->set_flag(key.data(), value == VxFlagBinary::ON);
  }

  uint64_t worldSeed() const { return api_->world_seed(); }

private:
  /// Pointer to the C ABI world domain function table.
  const PluginAPI::WorldDomain* api_;
};

}  // namespace eng
