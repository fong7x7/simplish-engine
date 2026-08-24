#pragma once

namespace eng::client {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// DesktopPlatformUtility: SDL3-backed PlatformUtility implementation.
//
// Responsibilities:
// - Register the desktop function-pointer table with PlatformUtility
// - Wrap SDL3 file dialog calls behind the platform-agnostic interface
// - Report desktop capabilities (has file dialog, no virtual keyboard)
//
// Key Invariants:
// - install() must be called exactly once during DesktopGameClient::init()
// - All SDL3 includes are confined to the .cpp file
//
// Threading: Main-thread only.
// ============================================================================

/// Registers the SDL3-backed PlatformUtility implementation.
/// @threading Main-thread only.
struct DesktopPlatformUtility {
  /// Register the desktop impl table with PlatformUtility.
  /// Call once during DesktopGameClient::init().
  static void install();
};

}  // namespace eng::client
