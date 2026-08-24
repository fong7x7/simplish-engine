#include "render_config_factory.h"

namespace eng::test {

eng::RenderConfig makeTestRenderConfig() {
  eng::RenderConfig cfg;
  cfg.preferred_backend = "vulkan";
  cfg.backbuffer_width = 320;
  cfg.backbuffer_height = 240;
  cfg.vsync = false;
  cfg.enable_validation = false;
  cfg.enable_ray_tracing = false;
  cfg.native_window = nullptr;
  return cfg;
}

eng::RenderConfig makeOpenGlRenderConfig() {
  eng::RenderConfig cfg = makeTestRenderConfig();
  cfg.preferred_backend = "opengl";
  return cfg;
}

}  // namespace eng::test
