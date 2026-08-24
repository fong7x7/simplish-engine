#pragma once

// Design Summary -- PS5 System Services
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/system-services.md
//
// Behaviours:
//   - Register suspend/resume event handler (SCE_USER_SERVICE_PRESENCE_UNKNOWN)
//   - Pause audio and networking on suspend; resume on wake
//   - Re-validate PSN session on resume
//   - No render or physics work during suspend
//   - Register Activity Cards: Continue World, New World
//   - Update activity cards when world changes
//
// Edge Cases:
//   - Suspend during multiplayer: pause sim, network handles disconnect
//   - Resume after long suspend: PSN re-validation, reconnect or offline
//   - Activity card for deleted world: error message, fall back to main menu
//
// Invariants:
//   - No render, physics, or audio during suspend
//   - PS5 SDK headers never in this public header
//
// Integration Points:
//   - Audio: paused/resumed on suspend events
//   - Networking: PSN re-validated on resume
//   - Game state machine: activity cards trigger state transitions
//   - EventBus: suspend/resume events emitted

#include "ps5-types.h"

#include <engine/core/event-bus.h>
#include <engine/core/expected-polyfill.h>
#include <string_view>

namespace eng {

// Forward declarations
struct Ps5PlatformContext;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Configuration for PS5 system services.
struct Ps5SystemConfig {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise PS5 system services (suspend/resume handler, activity cards).
/// Main thread only.
std::expected<void, Ps5Error> initPs5System(const Ps5SystemConfig& config);

/// Shut down PS5 system services.
/// Main thread only.
void shutdownPs5System();

/// Tick PS5 system events. Polls suspend/resume notifications and
/// dispatches events via EventBus. Call once per frame.
/// Main thread only.
void tickPs5System(Ps5PlatformContext& ctx);

/// Register an activity card on the PS5 home screen.
/// Main thread only.
void registerActivityCard(Ps5ActivityCardType type,
                          std::string_view world_seed);

/// Update an existing activity card (e.g. when the active world changes).
/// Main thread only.
void updateActivityCard(Ps5ActivityCardType type, std::string_view world_seed);

}  // namespace eng
