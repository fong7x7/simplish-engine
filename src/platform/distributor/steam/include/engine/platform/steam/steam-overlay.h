#pragma once

// Design Summary -- Steam Overlay
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/overlay.md
//
// Behaviours:
//   - Set the Steam overlay notification position (default bottom-right)
//   - Register overlay activation callback; emit engine event on open/close
//   - Hook screenshots so F12 triggers engine capture and Steam upload
//
// Edge Cases:
//   - Steam not available: all functions no-op
//   - Overlay disabled in Steam settings: activation callback never fires
//   - Screenshot hook before init: no-op
//   - Overlay in multiplayer: event fires; game code decides pause behaviour
//
// Invariants:
//   - Overlay hooks into swap chain automatically; no engine rendering code
//   needed
//   - Overlay activation emitted via EventBus; game code decides pause
//   - Main thread only
//
// Integration Points:
//   - EventBus: SteamOverlayActivated event for game pause logic
//   - Screenshot system: F12 delegated to engine screenshot capture

#include "steam-types.h"

namespace eng {

// Forward declaration
struct SteamContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Set where Steam overlay notifications appear on screen.
/// Default is BOTTOM_RIGHT to avoid HUD overlap.
/// Main thread only.
void setOverlayNotificationPosition(const SteamContext& ctx,
                                    SteamNotificationPosition position);

/// Hook the Steam screenshot system so pressing F12 triggers the
/// engine's screenshot capture and uploads the result to Steam.
/// Call once during init. No-op if Steam is unavailable.
/// Main thread only.
void hookScreenshots(const SteamContext& ctx);

}  // namespace eng
