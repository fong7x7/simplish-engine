#pragma once

#include <engine/render/rhi-draw-indexed-params.h>

namespace eng::render {

struct GlCmdDrawIndexed {
  /// Draw parameters.
  RhiDrawIndexedParams params{};
};

}  // namespace eng::render
