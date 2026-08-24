#pragma once

// Design Summary -- PS5 Platform Types
// Technical Approach: docs/technical-approaches/engine/platform-ps5.md
//
// Behaviours:
//   - Define strong ID types for PS5 users, sessions
//   - Define Ps5Error enum for all PS5 subsystem error reporting
//   - Define Ps5RenderMode, Ps5TriggerMode, Ps5TriggerSide enums
//   - Define Ps5NetworkStatus, Ps5ActivityCardType, Ps5AudioCodecPreference
//   enums
//   - Define Ps5HapticEvent, Ps5Button, Ps5ButtonGlyph enums
//   - Define Ps5TriggerEffectParams struct for adaptive trigger configuration
//   - Provide constants for GPU budget, max sources, max controllers
//
// Edge Cases:
//   - Invalid ID sentinel values for all strong ID types
//   - Ps5Error covers all subsystem failure modes in a single enum
//
// Invariants:
//   - No PS5 SDK types appear in this header
//   - All enums use uint8_t backing for compact storage
//   - Strong ID types prevent accidental mixing of user/session IDs
//
// Integration Points:
//   - All PS5 subsystem headers depend on this types header

#include "ps5-session-handle.h"
#include "ps5-trigger-effect-params.h"
#include "ps5-user-id.h"

#include <cstdint>

namespace eng {

// ---------------------------------------------------------------------------
// Error enum
// ---------------------------------------------------------------------------

enum class Ps5Error : uint8_t {
  NOT_AVAILABLE,
  INIT_FAILED,
  SDK_VERSION_MISMATCH,
  INVALID_ARGUMENT,

  // Graphics errors
  GNM_INIT_FAILED,
  GPU_MEMORY_EXHAUSTED,
  DIRECT_STORAGE_FAILED,
  FSR2_INIT_FAILED,

  // Audio errors
  TEMPEST_INIT_FAILED,
  SOURCE_POOL_EXHAUSTED,

  // Input errors
  PAD_INIT_FAILED,
  INVALID_TRIGGER_MODE,
  CONTROLLER_DISCONNECTED,

  // Networking errors
  PSN_SIGN_IN_FAILED,
  SESSION_CREATE_FAILED,
  INVITE_FAILED,
  NETWORK_UNAVAILABLE,
  PARENTAL_CONTROL_BLOCKED,

  // Save data errors
  SAVE_FAILED,
  LOAD_FAILED,
  SAVE_DATA_CORRUPTED,
  STORAGE_FULL,

  // Trophy errors
  TROPHY_INIT_FAILED,
  TROPHY_UNLOCK_FAILED,

  // System errors
  SYSTEM_INIT_FAILED,
};

// ---------------------------------------------------------------------------
// Render mode
// ---------------------------------------------------------------------------

enum class Ps5RenderMode : uint8_t {
  PERFORMANCE,  // 60 FPS at 1440p
  QUALITY,      // 30 FPS at 4K with FSR 2
};

// ---------------------------------------------------------------------------
// Network status
// ---------------------------------------------------------------------------

enum class Ps5NetworkStatus : uint8_t {
  OFFLINE,
  CONNECTING,
  ONLINE,
  SUSPENDED,
};

// ---------------------------------------------------------------------------
// Activity card type
// ---------------------------------------------------------------------------

enum class Ps5ActivityCardType : uint8_t {
  CONTINUE_WORLD,
  NEW_WORLD,
};

// ---------------------------------------------------------------------------
// Audio codec preference
// ---------------------------------------------------------------------------

enum class Ps5AudioCodecPreference : uint8_t {
  ATRAC9_PREFERRED,  // Use ATRAC9 when available, fall back to Vorbis
  VORBIS_ONLY,       // Force OGG Vorbis (for debugging/testing)
};

// ---------------------------------------------------------------------------
// DualSense haptic events (procedurally generated)
// ---------------------------------------------------------------------------

enum class Ps5HapticEvent : uint8_t {
  IMPACT_LIGHT,
  IMPACT_MEDIUM,
  IMPACT_HEAVY,
  SURFACE_DIRT,
  SURFACE_STONE,
  SURFACE_WATER,
  SURFACE_METAL,
  EXPLOSION_NEAR,
  EXPLOSION_FAR,
};

// ---------------------------------------------------------------------------
// PS-style button identifiers (for glyph lookup)
// ---------------------------------------------------------------------------

enum class Ps5Button : uint8_t {
  CROSS,
  CIRCLE,
  SQUARE,
  TRIANGLE,
  L1,
  R1,
  L2,
  R2,
  L3,
  R3,
  OPTIONS,
  CREATE,
  TOUCHPAD,
  DPAD_UP,
  DPAD_DOWN,
  DPAD_LEFT,
  DPAD_RIGHT,
};

// ---------------------------------------------------------------------------
// Button glyph (PS-style symbols for UI display)
// ---------------------------------------------------------------------------

enum class Ps5ButtonGlyph : uint8_t {
  GLYPH_CROSS,
  GLYPH_CIRCLE,
  GLYPH_SQUARE,
  GLYPH_TRIANGLE,
  GLYPH_L1,
  GLYPH_R1,
  GLYPH_L2,
  GLYPH_R2,
  GLYPH_L3,
  GLYPH_R3,
  GLYPH_OPTIONS,
  GLYPH_CREATE,
  GLYPH_TOUCHPAD,
  GLYPH_DPAD_UP,
  GLYPH_DPAD_DOWN,
  GLYPH_DPAD_LEFT,
  GLYPH_DPAD_RIGHT,
};

// ---------------------------------------------------------------------------
// Vibration toggle (replaces bare bool per coding standard)
// ---------------------------------------------------------------------------

enum class Ps5VibrationState : uint8_t {
  DISABLED,
  ENABLED,
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline constexpr uint32_t PS5_GPU_MEMORY_BUDGET_DEFAULT_MB = 6144;
inline constexpr uint32_t PS5_TEMPEST_MAX_SOURCES = 128;
inline constexpr uint32_t PS5_TEMPEST_DEFAULT_SAMPLE_RATE_HZ = 48000;
inline constexpr uint32_t PS5_MAX_CONTROLLERS = 4;
inline constexpr uint32_t PS5_MAX_SAVE_SLOTS = 64;
inline constexpr uint32_t PS5_STUB_DEFAULT_CONTROLLER_COUNT = 1;

}  // namespace eng
