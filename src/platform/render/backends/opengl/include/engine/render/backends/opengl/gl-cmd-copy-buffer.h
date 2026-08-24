#pragma once

#include <engine/render/rhi-copy-buffer-params.h>

namespace eng::render {

struct GlCmdCopyBuffer {
  /// Copy parameters.
  RhiCopyBufferParams params{};
};

}  // namespace eng::render
