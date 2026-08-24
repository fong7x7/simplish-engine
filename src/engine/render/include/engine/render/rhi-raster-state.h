#pragma once

namespace eng {

struct RhiRasterState {
  /// Whether wireframe rendering mode is active.
  bool wireframe = false;
  /// Whether back-face culling is enabled.
  bool cull_back = true;
  /// Whether front faces use counter-clockwise winding.
  bool front_ccw = true;
};

}  // namespace eng
