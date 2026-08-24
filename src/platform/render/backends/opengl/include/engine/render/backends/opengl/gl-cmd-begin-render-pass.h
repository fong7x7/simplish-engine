#pragma once

#include <engine/render/rhi-render-pass-begin-info.h>

namespace eng::render {

struct GlCmdBeginRenderPass {
  /// Render pass configuration.
  RhiRenderPassBeginInfo info{};
};

}  // namespace eng::render
