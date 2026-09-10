#include <engine/render-mesh/mesh-stand-in-texture.h>
#include <engine/render/rhi-texture-desc.h>

namespace eng {

RhiTextureHandle createMeshStandInTexture(RhiDevice& device) {
  RhiTextureDesc desc{};
  desc.width = 1;
  desc.height = 1;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.debug_name = "mesh_untextured";
  desc.initial_pixels = MESH_UNTEXTURED_TEXEL;
  return device.createTexture(desc);
}

}  // namespace eng
