#pragma once

// Design Summary -- Steam Input & Glyphs
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/input-glyphs.md
//
// Behaviours:
//   - Activate a Steam Input action set for a controller
//   - Retrieve platform-appropriate glyph path for an action origin
//   - Set controller rumble (left/right motors) via Steam Input
//   - Set DualSense adaptive trigger effects
//   - Show Steam Deck floating keyboard for text input
//   - Query connected controller handles and types
//
// Edge Cases:
//   - Steam Input unavailable: all functions no-op or return fallback
//   - No controllers connected: getControllerHandles() returns empty span
//   - Action set not found: return SteamError::ACTION_SET_NOT_FOUND
//   - Glyph for unknown origin: return empty string_view (caller uses fallback)
//   - DualSense effect on non-DualSense: no-op
//   - Floating keyboard on non-Deck: no-op
//
// Invariants:
//   - Action sets defined in IGA file; engine references by string name
//   - Glyph paths are filesystem paths to PNG files from Steamworks SDK
//   - Controller handle cache refreshed each frame in tickSteamCallbacks()
//   - Main thread only
//
// Integration Points:
//   - Engine Input System: Steam Input alongside SDL3 gamepad API
//   - GUI Framework: glyph paths rendered as controller button prompts

#include "steam-trigger-effect-params.h"
#include "steam-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <span>
#include <string_view>

namespace eng {

// Forward declaration
struct SteamContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Activate a Steam Input action set for the given controller.
/// Action set names match the IGA file uploaded to Steamworks partner portal
/// (e.g. "Gameplay", "Menu", "Editor", "Vehicle").
/// Main thread only.
std::expected<bool, SteamError>
activateActionSet(const SteamContext& ctx, SteamInputHandle controller,
                  std::string_view action_set_name);

/// Get the filesystem path to the glyph image for the given action on
/// the given controller. Returns empty string_view if the action is
/// unknown or Steam Input is unavailable (caller should use fallback atlas).
/// Main thread only.
std::string_view getGlyphForAction(const SteamContext& ctx,
                                   SteamInputHandle controller,
                                   std::string_view action_name);

/// Set rumble on the given controller. Values are 0-65535 for each motor.
/// Routes through Steam Input when available, falls back to SDL3 rumble.
/// Main thread only.
void setRumble(const SteamContext& ctx, SteamInputHandle controller,
               uint16_t left_motor, uint16_t right_motor);

/// Set DualSense adaptive trigger effect. No-op if the controller is not
/// a DualSense or Steam Input is unavailable.
/// Main thread only.
void setDualSenseTriggerEffect(const SteamContext& ctx,
                               SteamInputHandle controller,
                               SteamTriggerEffectMode mode,
                               const SteamTriggerEffectParams& params);

/// Show the Steam Deck floating on-screen keyboard. No-op on non-Deck
/// platforms (Steam handles gracefully).
/// Main thread only.
void showFloatingKeyboard(const SteamContext& ctx,
                          SteamFloatingKeyboardMode mode);

/// Returns a span of currently connected controller handles. The span
/// points to an internal cache refreshed each frame during
/// tickSteamCallbacks(). Valid until the next tick.
/// Main thread only.
std::span<const SteamInputHandle> getControllerHandles(const SteamContext& ctx);

}  // namespace eng
