#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace eng {

/// Canonical `RenderConfig::preferred_backend` token for OpenGL.
inline constexpr std::string_view RENDER_CONFIG_PREFERRED_BACKEND_OPENGL =
    "opengl";

struct RenderConfig {
  /// Preferred RHI backend name ("vulkan", "dx12",
  /// `RENDER_CONFIG_PREFERRED_BACKEND_OPENGL`, or "" for auto).
  std::string preferred_backend;
  /// Backbuffer width in pixels.
  uint32_t backbuffer_width = 1920;
  /// Backbuffer height in pixels.
  uint32_t backbuffer_height = 1080;
  /// Whether vertical sync is enabled.
  bool vsync = true;
  /// Enable GPU validation layers (Vulkan validation / D3D debug layer).
  bool enable_validation = false;
  /// Enable hardware ray tracing support if available.
  bool enable_ray_tracing = false;
  /// Native window handle (SDL_Window* or platform equivalent).
  void* native_window = nullptr;
};

}  // namespace eng
