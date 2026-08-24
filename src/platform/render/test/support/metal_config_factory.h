#pragma once

#include <engine/render/backends/metal/metal-rhi-config.h>

namespace eng::test {

/// Create a MetalRhiConfig suitable for unit tests (no window, validation off).
inline MetalRhiConfig makeTestMetalRhiConfig() {
  MetalRhiConfig cfg;
  cfg.max_frames_in_flight = 2;
  cfg.native_window = nullptr;
  cfg.backbuffer_width = 320;
  cfg.backbuffer_height = 240;
  cfg.vsync = false;
  cfg.enable_validation = false;
  cfg.enable_ray_tracing = false;
  return cfg;
}

/// Create a MetalRhiConfig with ray tracing enabled.
MetalRhiConfig makeTestMetalRhiConfigWithRayTracing();

/// Create a MetalRhiConfig with validation enabled.
MetalRhiConfig makeTestMetalRhiConfigWithValidation();

}  // namespace eng::test
