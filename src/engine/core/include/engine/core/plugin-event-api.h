#pragma once

#include "plugin-api.h"
#include "scoped-subscription.h"

#include <string_view>

namespace eng {

/// Type-safe C++ wrapper around the C ABI event subscription API.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class EventApi {
public:
  explicit EventApi(PluginAPI& api) : api_(&api) {}

  ScopedSubscription subscribe(std::string_view event_name,
                               void (*handler)(const char*, const void*,
                                               size_t)) {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    auto handle = api_->subscribe_event(event_name.data(), handler);
    return ScopedSubscription{handle, *api_};
  }

  void unsubscribe(SubscriptionHandle handle) {
    api_->unsubscribe_event(handle);
  }

private:
  /// Raw C ABI plugin API pointer for event operations.
  PluginAPI* api_;
};

}  // namespace eng
