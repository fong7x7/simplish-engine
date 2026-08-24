#include "metal_config_factory.h"

namespace eng::test {

MetalRhiConfig makeTestMetalRhiConfigWithRayTracing() {
  MetalRhiConfig cfg = makeTestMetalRhiConfig();
  cfg.enable_ray_tracing = true;
  return cfg;
}

MetalRhiConfig makeTestMetalRhiConfigWithValidation() {
  MetalRhiConfig cfg = makeTestMetalRhiConfig();
  cfg.enable_validation = true;
  return cfg;
}

}  // namespace eng::test
