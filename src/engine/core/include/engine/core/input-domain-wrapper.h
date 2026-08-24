#pragma once

#include "plugin-api.h"

#include <string_view>

namespace eng {

/// Type-safe C++ wrapper around the C ABI input domain function table.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class InputDomainWrapper {
public:
  explicit InputDomainWrapper(const PluginAPI::InputDomain& api) : api_(&api) {}

  bool isActionActive(std::string_view action_name) const {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    return api_->is_action_active(action_name.data());
  }

  float getAxisValue(std::string_view axis_name) const {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    return api_->get_axis_value(axis_name.data());
  }

private:
  /// Pointer to the C ABI input domain function table.
  const PluginAPI::InputDomain* api_;
};

}  // namespace eng
