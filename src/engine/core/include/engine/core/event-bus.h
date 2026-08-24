#pragma once

#include "engine-config.h"
#include "event-context.h"
#include "fnv1a.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// EventBus: Synchronous event dispatch with C ABI-safe payloads.
//
// Responsibilities:
// - Provide event subscription API (subscribe, unsubscribe)
// - Dispatch events synchronously on the thread that performs the action
// - Route events to all registered handlers
// - Guard against recursive emission of the same event
// - Isolation: Handler exceptions caught and logged
//
// Key Invariants:
// - Events dispatched synchronously (handlers complete before emit returns)
// - Handlers invoked on the thread calling emit() (usually main thread)
// - All plugin event handlers are main-thread-only
// - Recursive emission of same event guarded (log Warn, no-op)
// - Payloads use void* + size_t (C ABI safe at plugin boundary)
//
// Thread Safety:
// - subscribe/unsubscribe: main thread only
// - emit: any thread (handlers must be thread-safe for their thread)
// ============================================================================

class EventBus {
public:
  // Handler signature: C-compatible function pointer (C ABI safe)
  using EventHandler = void (*)(const EventContext& ctx);

  // Subscribe to an event; returns handle for later unsubscribe
  SubscriptionHandle subscribe(uint64_t event_id, EventHandler handler);

  // Unsubscribe by handle (idempotent; returns false if not found)
  bool unsubscribe(SubscriptionHandle handle);

  // Emit event synchronously with C ABI-safe payload.
  // Guards against recursive emission of same event.
  // Asserts that the event has been registered.
  void emit(uint64_t event_id, const void* payload, size_t payload_size);

  // List of all registered event names (for editor/debugging)
  std::unordered_map<uint64_t, std::string> registeredEvents() const;

  // Register event by name. Computes FNV-1a hash as the event ID.
  // Idempotent for the same name. Asserts on hash collision
  // (different names producing the same hash).
  uint64_t registerEvent(std::string_view event_name);

  // Count of handlers for event (for debugging)
  size_t handlerCount(uint64_t event_id) const;

  // Trace sink: a `std::function`-based subscriber that fires for *every*
  // emit, regardless of `event_id`. Mirrors `Logger::registerSink` in
  // shape and intent: an editor / diagnostic tool registers one trace,
  // captures the EventContext (including the registered name lookup
  // it needs) into its own buffer, and unsubscribes on shutdown.
  // Trace handlers run on the emitter thread (same threading guarantees
  // as `subscribe`). Engine-internal only; not exposed to plugins.
  using TraceHandler = std::function<void(const EventContext& ctx)>;
  SubscriptionHandle subscribeTrace(TraceHandler handler);
  bool unsubscribeTrace(SubscriptionHandle handle);
  size_t traceHandlerCount() const;

private:
  struct Subscription {
    /// Unique handle returned to the subscriber for later unsubscription.
    SubscriptionHandle handle{};
    /// Function pointer invoked when the subscribed event is emitted.
    EventHandler handler{};
  };
  /// Invoke all subscriber handlers for a given event context.
  static void dispatchToSubscribers(const std::vector<Subscription>& subs,
                                    const EventContext& ctx);
  /// Map from event id to its name
  std::unordered_map<uint64_t, std::string> registered_events_{};
  /// Map from event id to its list of active subscriptions.
  std::unordered_map<uint64_t, std::vector<Subscription>> subscriptions_{};
  /// Trace subscriber paired with its handle. Fires on every emit.
  struct TraceSubscription {
    /// Handle returned to the trace subscriber for later unsubscription.
    SubscriptionHandle handle{};
    /// Owning callable invoked for every event emission.
    TraceHandler handler{};
  };
  /// Active trace subscribers. Fires on every emit() before per-event
  /// dispatch. Empty by default; only diagnostic editor tooling
  /// registers a trace.
  std::vector<TraceSubscription> trace_subscribers_{};
  /// Monotonically increasing counter for generating unique subscription
  /// handles.
  SubscriptionHandle next_handle_ = 1;
  /// Event id currently being emitted (nullopt when idle); guards recursion.
  std::optional<uint64_t> currently_emitting_event_id_{};
};

}  // namespace eng
