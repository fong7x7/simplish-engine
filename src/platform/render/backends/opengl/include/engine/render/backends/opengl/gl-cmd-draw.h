#pragma once

#include <engine/render/rhi-draw-params.h>

namespace eng::render {

struct GlCmdDraw {
  /// Draw parameters.
  RhiDrawParams params{};
};

}  // namespace eng::render
