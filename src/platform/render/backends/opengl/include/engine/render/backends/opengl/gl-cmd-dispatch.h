#pragma once

#include <cstdint>

namespace eng::render {

struct GlCmdDispatch {
  /// Number of work groups in X.
  uint32_t groups_x = 1;
  /// Number of work groups in Y.
  uint32_t groups_y = 1;
  /// Number of work groups in Z.
  uint32_t groups_z = 1;
};

}  // namespace eng::render
