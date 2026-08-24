#pragma once

// Design Summary -- PS5 DualSense Input
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/dualsense-input.md
//
// Behaviours:
//   - Implement IPlatformInputBackend for DualSense via ScePad API
//   - Standard rumble via ScePad vibration motors mapped to setRumble()
//   - HD haptic waveforms procedurally generated from game events
//   - Adaptive trigger resistance profiles: off, feedback, vibration, weapon
//   - PS-style button glyphs for TRC-compliant button prompts
//   - Respect vibration on/off user setting (TRC requirement)
//   - Support up to 4 connected DualSense controllers
//
// Edge Cases:
//   - No controller connected: report no gamepad, fall back to KB/mouse
//   - Controller disconnected mid-session: emit event, pause game
//   - Vibration disabled in settings: all haptic/rumble calls are no-ops
//   - Invalid trigger mode: return error, do not change state
//   - Out-of-range trigger params: clamp to valid range, log warning
//   - Unknown surface type for haptic: use default impact waveform
//
// Invariants:
//   - ScePad headers never included in this public header
//   - Vibration setting checked before every haptic/rumble operation
//   - Button glyphs always PS-style symbols (TRC requirement)
//   - Main thread only
//
// Integration Points:
//   - IPlatformInputBackend: Ps5InputBackend is PS5 implementation
//   - InputSystem: injected via setPlatformBackend()
//   - EventBus: controller connect/disconnect events
//   - Settings: vibration toggle

#include "ps5-input-config.h"
#include "ps5-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <memory>

namespace eng {

// Forward declarations
struct IPlatformInputBackend;

// ---------------------------------------------------------------------------
// PS5 Input Backend
// ---------------------------------------------------------------------------

/// PS5 DualSense input backend implementing IPlatformInputBackend.
/// Created via static factory; returns nullptr on ScePad init failure.
/// All methods are main thread only.
class Ps5InputBackend {
public:
  /// Create and initialise the DualSense input backend. Returns nullptr
  /// if ScePad initialisation fails.
  /// Main thread only.
  static std::unique_ptr<Ps5InputBackend> create(const Ps5InputConfig& config);

  ~Ps5InputBackend();

  Ps5InputBackend(Ps5InputBackend&& other) noexcept;
  Ps5InputBackend& operator=(Ps5InputBackend&& other) noexcept;

  Ps5InputBackend(const Ps5InputBackend&) = delete;
  Ps5InputBackend& operator=(const Ps5InputBackend&) = delete;

  /// Enable or disable vibration at runtime. When disabled, all
  /// haptic and rumble calls become no-ops.
  /// Main thread only.
  void setVibrationState(Ps5VibrationState state);

  /// Returns the current vibration state.
  Ps5VibrationState vibrationState() const;

  /// Set the left and right rumble motor intensities [0.0, 1.0].
  /// No-op if vibration is disabled.
  /// Main thread only.
  void setRumble(float left, float right);

  /// Set an adaptive trigger effect on the specified trigger.
  /// Returns error if mode or params are invalid.
  /// No-op if vibration is disabled.
  /// Main thread only.
  std::expected<void, Ps5Error>
  setTriggerEffect(Ps5TriggerSide side, Ps5TriggerMode mode,
                   const Ps5TriggerEffectParams& params);

  /// Play a procedurally generated haptic event.
  /// No-op if vibration is disabled.
  /// Main thread only.
  void playHaptic(Ps5HapticEvent event);

  /// Get the PS-style button glyph for a given button.
  /// Always returns a valid PS glyph (TRC requirement).
  Ps5ButtonGlyph getButtonGlyph(Ps5Button button) const;

  /// Returns the number of currently connected controllers.
  uint32_t connectedControllerCount() const;

private:
  Ps5InputBackend();

  struct Impl;
  /// Opaque implementation holding ScePad state.
  std::unique_ptr<Impl> impl_;
};

}  // namespace eng
