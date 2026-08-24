#pragma once

#include <engine/render/rhi-scissor.h>

namespace eng::render {

struct GlCmdSetScissor {
  /// Scissor parameters.
  RhiScissor scissor{};
};

}  // namespace eng::render
