#pragma once

#include <engine/render/rhi-types.h>

namespace eng::render {

struct GlCmdBindPipeline {
  /// Pipeline handle to bind.
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
};

}  // namespace eng::render
