#pragma once

// Design Summary -- Xbox Series X Platform Types
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x.md
//
// Behaviours:
//   - Define strong ID types for Xbox users, sessions, sound handles
//   - Define XboxError enum for all Xbox subsystem error reporting
//   - Define XboxLifecycleState, XboxNetworkState, XboxPrivilegeResult enums
//   - Define XboxButton bitmask enum for controller state
//   - Define XboxMemoryPool, XboxReverbPreset, XboxTriggerSide enums
//   - Provide constants for memory budgets, max controllers, spatial audio
//   limits
//
// Edge Cases:
//   - Invalid ID sentinel values for all strong ID types
//   - XboxError covers all subsystem failure modes in a single enum
//
// Invariants:
//   - No GDK SDK types appear in this header
//   - All enums use uint8_t or uint32_t backing as appropriate
//   - Strong ID types prevent accidental mixing of user/session/sound IDs
//
// Integration Points:
//   - All Xbox subsystem headers depend on this types header

#include "xbox-session-handle.h"
#include "xbox-sound-handle.h"
#include "xbox-user-id.h"

#include <cstdint>

namespace eng {

// ---------------------------------------------------------------------------
// Error enum
// ---------------------------------------------------------------------------

enum class XboxError : uint8_t {
  NOT_AVAILABLE,
  INIT_FAILED,
  INVALID_ARGUMENT,

  // Lifecycle errors
  SUSPEND_FAILED,
  RESUME_FAILED,
  USER_SIGN_IN_FAILED,
  USER_NOT_SIGNED_IN,

  // DX12 errors
  DEVICE_CREATION_FAILED,
  SWAP_CHAIN_CREATION_FAILED,
  MEMORY_ALLOCATION_FAILED,
  DIRECT_STORAGE_INIT_FAILED,

  // Audio errors
  AUDIO_INIT_FAILED,
  AUDIO_SPATIAL_UNAVAILABLE,
  AUDIO_ALL_OBJECTS_IN_USE,

  // Input errors
  INPUT_INIT_FAILED,
  INPUT_DEVICE_NOT_FOUND,

  // Networking / Xbox Live errors
  LIVE_NOT_AVAILABLE,
  SESSION_CREATE_FAILED,
  PRIVILEGE_DENIED,
  NETWORK_DISCONNECTED,

  // Save data errors
  SAVE_PROVIDER_FAILED,
  SAVE_WRITE_FAILED,
  SAVE_READ_FAILED,
  SAVE_NOT_FOUND,
  SAVE_CORRUPTED,
  STORAGE_FULL,
  SAVE_SYNC_CONFLICT,

  // Achievement errors
  ACHIEVEMENT_UPDATE_FAILED,
};

// ---------------------------------------------------------------------------
// Lifecycle state
// ---------------------------------------------------------------------------

enum class XboxLifecycleState : uint8_t {
  RUNNING,
  SUSPENDING,
  SUSPENDED,
  RESUMING,
};

// ---------------------------------------------------------------------------
// Network state
// ---------------------------------------------------------------------------

enum class XboxNetworkState : uint8_t {
  UNKNOWN,
  CONNECTED,
  DISCONNECTED,
};

// ---------------------------------------------------------------------------
// Privilege check result
// ---------------------------------------------------------------------------

enum class XboxPrivilegeResult : uint8_t {
  ALLOWED,
  DENIED,
  NOT_SIGNED_IN,
};

// ---------------------------------------------------------------------------
// Xbox privilege types
// ---------------------------------------------------------------------------

enum class XboxPrivilege : uint8_t {
  ONLINE_MULTIPLAYER,
  COMMUNICATIONS,
  USER_GENERATED_CONTENT,
};

// ---------------------------------------------------------------------------
// Controller button bitmask
// ---------------------------------------------------------------------------

enum class XboxButton : uint32_t {
  NONE = 0,
  A = 1U << 0,
  B = 1U << 1,
  X = 1U << 2,
  Y = 1U << 3,
  DPAD_UP = 1U << 4,
  DPAD_DOWN = 1U << 5,
  DPAD_LEFT = 1U << 6,
  DPAD_RIGHT = 1U << 7,
  LEFT_SHOULDER = 1U << 8,
  RIGHT_SHOULDER = 1U << 9,
  LEFT_STICK = 1U << 10,
  RIGHT_STICK = 1U << 11,
  START = 1U << 12,
  BACK = 1U << 13,
};

// ---------------------------------------------------------------------------
// Memory pool type (DX12 helpers)
// ---------------------------------------------------------------------------

enum class XboxMemoryPool : uint8_t {
  FAST,      // 10 GB @ 560 GB/s -- render targets, textures, frame buffers
  STANDARD,  // 6 GB @ 336 GB/s -- CPU-side data, audio buffers
};

// ---------------------------------------------------------------------------
// Audio reverb presets
// ---------------------------------------------------------------------------

enum class XboxReverbPreset : uint8_t {
  NONE,
  CAVE,
  INDOOR,
  OUTDOOR,
  UNDERWATER,
};

// ---------------------------------------------------------------------------
// Trigger side for impulse triggers
// ---------------------------------------------------------------------------

enum class XboxTriggerSide : uint8_t {
  LEFT,
  RIGHT,
};

// ---------------------------------------------------------------------------
// Vibration toggle
// ---------------------------------------------------------------------------

enum class XboxVibrationToggle : uint8_t {
  DISABLED,
  ENABLED,
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline constexpr uint32_t XBOX_MAX_CONTROLLERS = 8;

}  // namespace eng
