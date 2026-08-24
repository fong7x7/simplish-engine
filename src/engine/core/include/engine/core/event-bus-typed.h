#pragma once

#include "event-bus.h"
#include "event-context.h"
#include "event-trait.h"

#include <cstddef>
#include <engine/core/assert.h>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Typed event emission and subscription wrappers for EventBus.
//
// Provides compile-time type safety over the untyped EventBus API.
// Uses EventTrait<T> to resolve event IDs at compile time.
// Engine-internal only; plugins use the C ABI PluginAPI.
//
// Thread Safety:
// - Same as EventBus: emit from any thread, subscribe main-thread-only.
// ============================================================================

/// Emit a typed event. Registers the event if not already registered.
/// T must have a valid EventTrait<T> specialization.
template <typename T> void emitTyped(EventBus& bus, const T& event) {
  bus.registerEvent(EventTrait<T>::NAME);
  bus.emit(EventTrait<T>::ID, &event, sizeof(T));
}

/// Trampoline: static function template that casts the type-erased
/// payload to const T& and forwards to a compile-time handler.
/// Handler must be a non-capturing function pointer known at compile time.
template <typename T, void (*Handler)(const T&)>
void typedTrampoline(const EventContext& ctx) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while) -- macro idiom
  ENGINE_ASSERT(ctx.payload_size == sizeof(T),
                "payload size mismatch for typed event trampoline");
  const auto* typed = static_cast<const T*>(ctx.payload);
  Handler(*typed);
}

/// Subscribe to a typed event with a compile-time handler.
/// Registers the event if not already registered.
/// Usage: subscribeTyped<MyEvent, &myHandler>(bus)
template <typename T, void (*Handler)(const T&)>
SubscriptionHandle subscribeTyped(EventBus& bus) {
  bus.registerEvent(EventTrait<T>::NAME);
  return bus.subscribe(EventTrait<T>::ID, &typedTrampoline<T, Handler>);
}

}  // namespace eng
