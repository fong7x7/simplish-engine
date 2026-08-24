#pragma once

#include "engine-config.h"
#include "plugin-api.h"

namespace eng {

// ============================================================================
// DESIGN SUMMARY — ScopedSubscription
// Technical Approach: docs/technical-approaches/engine/plugin-cpp-wrappers.md
//
// Behaviours:
//   - RAII wrapper for PluginAPI event subscription handles
//   - Unsubscribes automatically on destruction
//   - Move-only; transfer ownership between scopes
//   - release() detaches handle without unsubscribing
//
// Edge Cases:
//   - Moved-from instance holds INVALID handle; destructor is a no-op
//   - release() returns handle and clears internal state
//   - Destroyed after PluginAPI invalidated: UB (documented precondition)
//
// Invariants:
//   - At most one ScopedSubscription owns a given handle
//   - No heap allocation
//
// Integration Points:
//   - PluginAPI::unsubscribe_event — called on destruction
//   - EventApi — constructs ScopedSubscription from subscribe result
// ============================================================================

/// RAII handle for a plugin event subscription.
/// Automatically unsubscribes when destroyed.
///
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class ScopedSubscription {
public:
  /// Construct from a subscription handle and the API used to unsubscribe.
  ScopedSubscription(SubscriptionHandle handle, PluginAPI& api)
    : handle_(handle), api_(&api) {}

  /// Unsubscribe on destruction if handle is still owned.
  ~ScopedSubscription() {
    if (api_ != nullptr && handle_ != 0) {
      api_->unsubscribe_event(handle_);
    }
  }

  /// Move constructor — transfers ownership from other.
  ScopedSubscription(ScopedSubscription&& other) noexcept;

  /// Move assignment — unsubscribes current, takes ownership from other.
  ScopedSubscription& operator=(ScopedSubscription&& other) noexcept;

  ScopedSubscription(const ScopedSubscription&) = delete;
  ScopedSubscription& operator=(const ScopedSubscription&) = delete;

  /// Get the underlying subscription handle.
  SubscriptionHandle handle() const { return handle_; }

  /// Release ownership without unsubscribing; returns the handle.
  SubscriptionHandle release();

private:
  /// The subscription handle returned by subscribe_event.
  SubscriptionHandle handle_ = 0;
  /// The PluginAPI used to unsubscribe; nullptr if released or moved-from.
  PluginAPI* api_ = nullptr;
};

}  // namespace eng
