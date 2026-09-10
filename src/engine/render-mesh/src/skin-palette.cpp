#include <algorithm>
#include <engine/render-mesh/skin-palette.h>

namespace eng {

namespace {

  /// Write the top three rows of @p m as joint @p joint's rows.
  void writeJointRows(const Mat4& m, size_t joint, SkinPalette& palette) {
    for (size_t row = 0; row < SKIN_PALETTE_ROWS_PER_JOINT; ++row) {
      float* out = palette.rows[joint * SKIN_PALETTE_ROWS_PER_JOINT + row];
      for (size_t column = 0; column < 4; ++column) {
        out[column] = m(row, column);
      }
    }
  }

}  // namespace

SkinPalette makeSkinPalette(std::span<const Mat4> skin) {
  SkinPalette palette;
  const size_t count = std::min(skin.size(), MESH_MAX_SKIN_JOINTS);
  const Mat4 identity = Mat4::identity();
  for (size_t joint = 0; joint < MESH_MAX_SKIN_JOINTS; ++joint) {
    writeJointRows(joint < count ? skin[joint] : identity, joint, palette);
  }
  return palette;
}

}  // namespace eng
