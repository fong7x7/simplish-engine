#pragma once

#include <cstdint>
#include <engine/render/rhi-vertex-attribute.h>

namespace eng {

struct RhiVertexLayout {
  /// Byte stride between consecutive vertices.
  uint32_t stride = 0;
  /// Pointer to the array of vertex attributes.
  const RhiVertexAttribute* attributes = nullptr;
  /// Number of vertex attributes in the array.
  uint32_t attribute_count = 0;
};

}  // namespace eng
