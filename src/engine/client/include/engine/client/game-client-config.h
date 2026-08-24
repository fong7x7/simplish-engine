#pragma once

#include <cstdint>
#include <string>

namespace eng::client {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// GameClientConfig: Configuration for game client window and engine startup.
//
// Responsibilities:
// - Define window dimensions, title, and constraints
// - Specify engine data directory and rendering preferences
//
// Key Invariants:
// - All fields have sensible defaults
// - Read-only after game client construction
// - No environment-specific hardcoded values
//
// Thread Safety:
// - Main thread only (used only during init).
// ============================================================================

struct GameClientConfig {
  /// Window title shown in the OS title bar.
  std::string window_title = "Simplish";
  /// Initial window width in pixels.
  uint32_t window_width = 1920;
  /// Initial window height in pixels.
  uint32_t window_height = 1080;
  /// Minimum allowed window width in pixels.
  uint32_t min_window_width = 640;
  /// Minimum allowed window height in pixels.
  uint32_t min_window_height = 480;
  /// Root data directory for engine assets (must be non-empty for init).
  /// Launches typically set ENGINE_DATA_DIR or default to relative "data".
  std::string data_dir{};
  /// Preferred RHI backend ("vulkan", "dx12", "opengl", or "" for auto).
  std::string preferred_backend{};
  /// Whether vertical sync is enabled.
  bool vsync = true;
};

}  // namespace eng::client
