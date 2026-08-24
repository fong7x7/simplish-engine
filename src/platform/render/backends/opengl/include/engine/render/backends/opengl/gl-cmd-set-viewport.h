#pragma once

#include <engine/render/rhi-viewport.h>

namespace eng::render {

struct GlCmdSetViewport {
  /// Viewport parameters.
  RhiViewport viewport{};
};

}  // namespace eng::render
