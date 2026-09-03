#include <algorithm>
#include <editor/shell/editor-placement-transform.h>

namespace eng::editor {

namespace {

  /// Scale that makes the larger horizontal extent one tile across.
  float footprintScale(const EditorAsset& asset) {
    const float width = asset.max.x - asset.min.x;
    const float depth = asset.max.y - asset.min.y;
    const float footprint = std::max(width, depth);
    // A degenerate or unmeasured mesh keeps its own units rather than
    // scaling by infinity.
    return footprint > 0.0f ? 1.0f / footprint : 1.0f;
  }

}  // namespace

Mat4 makePlacementTransform(const EditorAsset& asset, WorldPoint position) {
  const float scale = footprintScale(asset);
  const float center_x = (asset.min.x + asset.max.x) * 0.5f;
  const float center_y = (asset.min.y + asset.max.y) * 0.5f;
  Mat4 out{};
  out(0, 0) = scale;
  out(1, 1) = scale;
  out(2, 2) = scale;
  // Centre the footprint on the tile, and rest the lowest point on z = 0.
  out(0, 3) = position.x + 0.5f - center_x * scale;
  out(1, 3) = position.y + 0.5f - center_y * scale;
  out(2, 3) = position.z - asset.min.z * scale;
  out(3, 3) = 1.0f;
  return out;
}

}  // namespace eng::editor
