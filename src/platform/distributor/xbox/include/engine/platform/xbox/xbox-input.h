#pragma once

// Design Summary -- Xbox Series X Input (GameInput)
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x/input.md
//
// Behaviours:
//   - Poll controller state via GDK GameInput each frame
//   - Map GameInput readings to engine's unified input action system
//   - Support Xbox Wireless, Elite, and third-party controllers
//   - Support keyboard and mouse when connected via GameInput
//   - Set controller rumble (left/right motors)
//   - Set impulse trigger vibration (left/right triggers)
//   - input_set_trigger_effect() is a no-op on Xbox (no adaptive resistance)
//   - Provide Xbox-style button glyphs (A/B/X/Y)
//   - Support vibration on/off setting (XR certification)
//
// Edge Cases:
//   - No controller connected: return default-zeroed state
//   - Controller disconnected mid-gameplay: emit event
//   - Multiple controllers (up to 8): local multiplayer support
//   - Keyboard/mouse not connected: empty readings, no error
//   - Impulse trigger on non-Xbox controller: no-op
//   - Rumble disabled in settings: rumble functions are no-ops
//
// Invariants:
//   - GameInput polled exactly once per frame on main thread
//   - Controller ID stable across frames for same physical device
//   - Impulse triggers provide vibration only, not resistance
//   - Vibration setting checked before every rumble call
//
// Integration Points:
//   - Engine input system: XboxInputBackend implements IInputBackend
//   - GUI framework: Xbox button glyphs for controller prompts
//   - Settings system: vibration on/off toggle

#include "xbox-controller-state.h"
#include "xbox-input-config.h"
#include "xbox-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>

namespace eng {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise GameInput. Must be called after GDK runtime init.
/// Main thread only.
std::expected<bool, XboxError> initXboxInput(const XboxInputConfig& config);

/// Shut down GameInput. Releases all device references. Idempotent.
/// Main thread only.
void shutdownXboxInput();

/// Poll the current state of a controller by index (0-based).
/// Returns a default-zeroed state with connected=false if the
/// controller is not connected.
/// Main thread only.
XboxControllerState pollXboxInput(uint32_t controller_index);

/// Set rumble on a controller. left and right are 0.0 to 1.0.
/// No-op if vibration is disabled or the controller is not connected.
/// Main thread only.
void setXboxRumble(uint32_t controller_index, float left, float right);

/// Set impulse trigger vibration on one trigger. Intensity is 0.0 to 1.0.
/// Xbox impulse triggers provide vibration only (not adaptive resistance).
/// No-op if vibration is disabled, the controller lacks impulse triggers,
/// or the controller is not connected.
/// Main thread only.
void setXboxTriggerRumble(uint32_t controller_index, XboxTriggerSide side,
                          float intensity);

/// Enable or disable vibration globally. When disabled, all rumble
/// and trigger vibration calls become no-ops. XR certification requires
/// this setting to be user-accessible.
/// Main thread only.
void setXboxVibrationEnabled(XboxVibrationToggle toggle);

/// Returns the number of currently connected controllers (0 to 8).
/// Main thread only.
uint32_t getXboxConnectedControllerCount();

}  // namespace eng
