#pragma once

#include "plugin-api.h"

#include <string_view>

namespace eng {

/// Type-safe C++ wrapper around the C ABI audio domain function table.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class AudioDomainWrapper {
public:
  explicit AudioDomainWrapper(const PluginAPI::AudioDomain& api) : api_(&api) {}

  void playSound(std::string_view path, const Vec3& pos) {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    api_->play_sound(path.data(), &pos);
  }

private:
  /// Pointer to the C ABI audio domain function table.
  const PluginAPI::AudioDomain* api_;
};

}  // namespace eng
