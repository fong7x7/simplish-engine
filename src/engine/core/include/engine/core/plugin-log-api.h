#pragma once

#include "plugin-api.h"

#include <string>
#include <string_view>

namespace eng {

/// Type-safe C++ wrapper around the C ABI plugin logging API.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class LogApi {
public:
  LogApi(PluginAPI& api, std::string_view plugin_id)
    : api_(&api), plugin_id_(plugin_id) {}

  void info(std::string_view message) const {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    api_->log_info(plugin_id_.c_str(), message.data());
  }

  void warn(std::string_view message) const {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    api_->log_warn(plugin_id_.c_str(), message.data());
  }

  void error(std::string_view message) const {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    api_->log_error(plugin_id_.c_str(), message.data());
  }

private:
  /// Raw C ABI plugin API pointer for log calls.
  PluginAPI* api_;
  /// Unique identifier of the owning plugin.
  std::string plugin_id_;
};

}  // namespace eng
