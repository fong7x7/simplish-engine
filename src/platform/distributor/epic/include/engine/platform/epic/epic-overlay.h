#pragma once

// Design Summary -- Epic Overlay
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/overlay.md
//
// Behaviours:
//   - Set notification position (default bottom-right)
//   - Query overlay presence
//   - Overlay activation emitted as EventBus event internally
//
// Edge Cases:
//   - EOS not available: all functions no-op
//   - Overlay not present (non-EGS launch): returns false
//
// Invariants:
//   - Overlay only available when launched through EGS client
//   - Overlay callbacks fire on main thread during EOS_Platform_Tick()
//
// Integration Points:
//   - Engine game loop: overlay activation pauses/resumes simulation
//   - EventBus: on_epic_overlay_activated(bool) emitted

#include "epic-types.h"

namespace eng {

// Forward declarations
struct EpicContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Set the position for Epic overlay notifications. Default is
/// bottom-right. No-op if EOS or overlay is unavailable.
/// Main thread only.
void epicSetNotificationPosition(const EpicContext& ctx,
                                 EpicNotificationPosition position);

/// Returns true if the Epic overlay is present and functional.
/// Returns false if EOS is unavailable or game was not launched
/// through the EGS client.
/// Main thread only.
bool epicOverlayPresent(const EpicContext& ctx);

}  // namespace eng
