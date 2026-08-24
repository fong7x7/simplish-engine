#pragma once

#include <cstdint>

namespace eng {

/// @brief Type tag for draw commands.
/// @thread_safety Immutable value type.
enum class DrawCommandType : uint8_t {
  QUAD_BATCH,
  PUSH_SCISSOR,
  POP_SCISSOR,
};

}  // namespace eng
