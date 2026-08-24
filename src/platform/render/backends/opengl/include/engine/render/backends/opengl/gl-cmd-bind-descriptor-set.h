#pragma once

#include <engine/render/rhi-types.h>

namespace eng::render {

struct GlCmdBindDescriptorSet {
  /// Descriptor set slot index.
  uint32_t set_index = 0;
  /// Descriptor set handle.
  RhiDescriptorSetHandle set = RHI_DESCRIPTOR_SET_INVALID;
};

}  // namespace eng::render
