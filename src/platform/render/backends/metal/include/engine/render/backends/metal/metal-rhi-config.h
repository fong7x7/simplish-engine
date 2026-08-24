#pragma once

#ifdef ENGINE_RENDERER_METAL

// Design Summary -- Metal RHI Configuration
// Technical Approach:
// docs/technical-approaches/engine/rendering/metal-rhi-backend.md
//
// Behaviours:
//   - Provide Metal-specific configuration for RHI device creation
//   - Built from RenderConfig at device creation time
//
// Invariants:
//   - No Metal/Objective-C types in this header
//   - Passed as const& to MetalRhiDevice::create()

#include <cstdint>

namespace eng {

/// Configuration for the Metal RHI backend on macOS.
struct MetalRhiConfig {
  /// Maximum number of frames the CPU can prepare ahead of the GPU.
  uint32_t max_frames_in_flight = 2;
  /// Native window handle (SDL_Window*) for CAMetalLayer attachment.
  void* native_window = nullptr;
  /// Backbuffer width in pixels.
  uint32_t backbuffer_width = 1920;
  /// Backbuffer height in pixels.
  uint32_t backbuffer_height = 1080;
  /// Enable display sync on CAMetalLayer.
  bool vsync = true;
  /// Enable Metal validation layer for debugging.
  bool enable_validation = false;
  /// Request hardware ray tracing if the GPU supports it.
  bool enable_ray_tracing = false;
};

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
