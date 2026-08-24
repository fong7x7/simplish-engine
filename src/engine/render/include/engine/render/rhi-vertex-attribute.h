#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

struct RhiVertexAttribute {
  /// Shader input location index for this attribute.
  uint32_t location = 0;
  /// Data format of this vertex attribute.
  RhiFormat format = RhiFormat::RGB_A32_FLOAT;
  /// Byte offset of this attribute within the vertex stride.
  uint32_t offset = 0;
};

}  // namespace eng
