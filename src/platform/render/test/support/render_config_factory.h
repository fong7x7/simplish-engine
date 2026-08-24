#pragma once

#include <engine/render/render-config.h>

namespace eng::test {

/// Create a RenderConfig for platform factory tests (no window handle).
inline eng::RenderConfig makePlatformTestRenderConfig() {
  eng::RenderConfig cfg;
  cfg.preferred_backend = "";
  cfg.backbuffer_width = 320;
  cfg.backbuffer_height = 240;
  cfg.vsync = false;
  cfg.enable_validation = false;
  cfg.enable_ray_tracing = false;
  cfg.native_window = nullptr;
  return cfg;
}

/// Create a RenderConfig suitable for unit tests (no window, validation off).
eng::RenderConfig makeTestRenderConfig();

/// Create a RenderConfig requesting OpenGL fallback.
eng::RenderConfig makeOpenGlRenderConfig();

}  // namespace eng::test
