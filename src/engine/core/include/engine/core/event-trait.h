#pragma once

#include <cstdint>
#include <engine/core/fnv1a.h>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// EventTrait<T>: Compile-time binding of an event payload type to its ID
// and canonical name.
//
// Intentionally left undefined. Each event payload header must provide a
// specialization via the ENGINE_EVENT_TRAIT macro:
//
//   ENGINE_EVENT_TRAIT(MyEvent, "my.event");
//
// The macro generates a full specialization that binds ID (FNV-1a hash of the
// event name string) and NAME (that same string literal).
//
// Attempting to use EventTrait with an unspecialized type produces a
// compile error, enforcing that every event type declares its binding.
//
// Thread Safety: Compile-time only, no runtime state.
// ============================================================================

template <typename T> struct EventTrait;

}  // namespace eng

// ---------------------------------------------------------------------------
// Convenience macro for declaring EventTrait specializations.
// Place after the struct definition, inside the vx namespace.
// The event name string appears once, eliminating ID/NAME desync risk.
// ---------------------------------------------------------------------------
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage) -- declaration shorthand, not
// logic
#define ENGINE_EVENT_TRAIT(PayloadType, EventName)                             \
  template <> struct EventTrait<PayloadType> {                                 \
    static constexpr uint64_t ID = fnv1a(EventName);                           \
    static constexpr std::string_view NAME = EventName;                        \
  }
