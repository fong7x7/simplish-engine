#pragma once

// Design Summary -- PS5 Trophies
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/system-services.md
//
// Behaviours:
//   - Unlock trophies via sceNpTrophyUnlockTrophy()
//   - Fire-and-forget unlock; no blocking wait for server confirmation
//   - Trophy unlock is idempotent (re-unlocking is a no-op)
//
// Edge Cases:
//   - Trophy server unreachable: queue unlock, retry on next tick
//   - Trophy already unlocked: no-op
//
// Invariants:
//   - Unlocks are idempotent
//   - PS5 SDK headers never in this public header
//
// Integration Points:
//   - Game achievement system: calls unlockTrophy()
//   - EventBus: trophy unlock events

#include "ps5-types.h"

#include <engine/core/event-bus.h>
#include <engine/core/expected-polyfill.h>
#include <string_view>

namespace eng {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Configuration for PS5 trophy subsystem.
struct Ps5TrophyConfig {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise the PS5 trophy subsystem.
/// Main thread only.
std::expected<void, Ps5Error> initPs5Trophies(const Ps5TrophyConfig& config);

/// Shut down the PS5 trophy subsystem.
/// Main thread only.
void shutdownPs5Trophies();

/// Unlock a trophy by string ID. Fire-and-forget: the unlock is queued
/// and will be retried if the trophy server is unreachable. Idempotent:
/// unlocking an already-unlocked trophy is a no-op.
/// Main thread only.
void unlockTrophy(std::string_view trophy_id);

}  // namespace eng
