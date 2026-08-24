#pragma once

#include "draw-command-type.h"
#include "quad-batch-cmd.h"
#include "scissor-cmd.h"

namespace eng {

/// @brief Batched draw command for the GUI renderer.
/// @thread_safety Immutable value type.
struct DrawCommand {
  /// Type of draw command (quad batch, push/pop scissor).
  DrawCommandType type = DrawCommandType::QUAD_BATCH;
  /// Quad batch parameters (valid when type is QuadBatch).
  QuadBatchCmd batch;
  /// Scissor rect parameters (valid when type is PushScissor/PopScissor).
  ScissorCmd scissor;
};

}  // namespace eng
