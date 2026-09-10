#pragma once

#ifdef ENGINE_RENDERER_OPENGL

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// GlCommand: variant type for deferred OpenGL command recording.
//
// Responsibilities:
// - Define one struct per recordable GPU operation
// - Compose all command structs into a single std::variant (GlCommand)
// - Used by OpenGlCommandList to store recorded commands
// - Replayed by OpenGlDevice::submit() to issue actual GL calls
//
// Key Invariants:
// - Each command struct is a plain data type with no behaviour
// - GlCommand variant covers all RhiCommandList operations
// - Commands reference RHI handles; OpenGlDevice resolves to GL objects
//
// Threading:
// - Command structs are value types; safe to move between contexts
// ============================================================================

#include "gl-cmd-begin-render-pass.h"
#include "gl-cmd-bind-descriptor-set.h"
#include "gl-cmd-bind-fragment-texture.h"
#include "gl-cmd-bind-index-buffer.h"
#include "gl-cmd-bind-pipeline.h"
#include "gl-cmd-bind-vertex-buffer.h"
#include "gl-cmd-copy-buffer.h"
#include "gl-cmd-copy-texture-to-buffer.h"
#include "gl-cmd-dispatch.h"
#include "gl-cmd-draw-indexed.h"
#include "gl-cmd-draw.h"
#include "gl-cmd-end-render-pass.h"
#include "gl-cmd-set-fragment-stage-bytes.h"
#include "gl-cmd-set-scissor.h"
#include "gl-cmd-set-vertex-stage-block.h"
#include "gl-cmd-set-vertex-stage-bytes.h"
#include "gl-cmd-set-viewport.h"
#include "gl-cmd-texture-barrier.h"

#include <variant>

namespace eng::render {

using GlCommand =
    std::variant<GlCmdBeginRenderPass, GlCmdEndRenderPass, GlCmdBindPipeline,
                 GlCmdBindVertexBuffer, GlCmdBindIndexBuffer,
                 GlCmdSetVertexStageBytes, GlCmdSetVertexStageBlock,
                 GlCmdSetFragmentStageBytes, GlCmdBindFragmentTexture,
                 GlCmdBindDescriptorSet, GlCmdSetViewport, GlCmdSetScissor,
                 GlCmdDraw, GlCmdDrawIndexed, GlCmdDispatch, GlCmdCopyBuffer,
                 GlCmdCopyTextureToBuffer, GlCmdTextureBarrier>;

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
