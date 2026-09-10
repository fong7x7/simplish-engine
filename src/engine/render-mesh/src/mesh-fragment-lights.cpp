#include <algorithm>
#include <cstring>
#include <engine/render-mesh/mesh-fragment-lights.h>

namespace eng {

MeshFragmentLights makeMeshFragmentLights(std::span<const MeshLight> lights,
                                          uint32_t shade_bands) {
  MeshFragmentLights block;
  block.shade_bands = shade_bands;
  if (lights.empty()) {
    block.count = 1;
    return block;
  }
  block.count = static_cast<uint32_t>(std::min(lights.size(), MESH_MAX_LIGHTS));
  std::memcpy(block.lights, lights.data(), block.count * sizeof(MeshLight));
  return block;
}

}  // namespace eng
