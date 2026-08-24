#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <thread>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// EventContext: Payload passed to event handlers during dispatch.
//
// Provides the event id, timestamp, emitter thread, and a type-erased
// pointer to the event payload. Handlers cast the payload based on event_id.
//
// Thread Safety:
// - Read-only struct; safe from any thread.
// ============================================================================

struct EventContext {
  /// id of the event being dispatched.
  uint64_t event_id{};
  /// Timestamp in milliseconds when the event was emitted.
  int64_t timestamp_ms{};
  /// Thread ID of the thread that emitted the event.
  std::thread::id emitter_thread{};
  /// Pointer to the event payload; typed by event_id.
  const void* payload{};
  /// Size of the payload in bytes.
  size_t payload_size{};
};

}  // namespace eng
