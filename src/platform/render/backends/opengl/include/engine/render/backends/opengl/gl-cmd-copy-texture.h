#pragma once

/// @file gl-cmd-copy-texture.h
/// @brief A recorded `copyTexture`.
/// @par Threading
/// Main-thread-only, as the command list recording it.

#include <engine/render/rhi-types.h>

namespace eng::render {

/// Copy what the passes drew into @p dst. Every pass draws into the default
/// framebuffer here, so that is what is read, whatever @p src names.
struct GlCmdCopyTexture {
  /// Source texture handle — the backbuffer, as the caller saw it.
  RhiTextureHandle src = RHI_TEXTURE_INVALID;
  /// Destination texture handle.
  RhiTextureHandle dst = RHI_TEXTURE_INVALID;
};

}  // namespace eng::render
