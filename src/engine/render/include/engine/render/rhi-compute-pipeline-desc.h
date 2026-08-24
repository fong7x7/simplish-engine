#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

struct RhiComputePipelineDesc {
  /// Compiled compute shader handle for this pipeline.
  RhiShaderHandle compute_shader = RHI_SHADER_INVALID;
  /// Optional debug label shown in GPU profilers.
  const char* debug_name = nullptr;
};

}  // namespace eng
