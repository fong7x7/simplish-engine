#pragma once

namespace eng {

/// @thread_safety Main thread only.
struct ScissorCmd {
  /// Left edge of the scissor rectangle.
  float x = 0.0f;
  /// Top edge of the scissor rectangle.
  float y = 0.0f;
  /// Width of the scissor rectangle.
  float w = 0.0f;
  /// Height of the scissor rectangle.
  float h = 0.0f;
};

}  // namespace eng
