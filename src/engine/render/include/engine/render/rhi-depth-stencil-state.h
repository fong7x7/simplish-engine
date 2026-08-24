#pragma once

namespace eng {

struct RhiDepthStencilState {
  /// Whether depth testing is enabled.
  bool depth_test = true;
  /// Whether depth writes are enabled.
  bool depth_write = true;
};

}  // namespace eng
