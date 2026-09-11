#include <algorithm>
#include <cmath>
#include <editor/shell/editor-placement-transform.h>
#include <engine/math/math.h>

namespace eng::editor {

namespace {

  constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;

  /// Scale that makes the larger horizontal extent one tile across.
  float footprintScale(const EditorAsset& asset) {
    const float width = asset.max.x - asset.min.x;
    const float depth = asset.max.y - asset.min.y;
    const float footprint = std::max(width, depth);
    // A degenerate or unmeasured mesh keeps its own units rather than
    // scaling by infinity.
    return footprint > 0.0f ? 1.0f / footprint : 1.0f;
  }

  /// Whether the asset's bounds were ever measured. They are filled in on
  /// upload, so an asset that has never reached the GPU has none.
  bool hasMeasuredBounds(const EditorAsset& asset) {
    return asset.max.x > asset.min.x || asset.max.y > asset.min.y ||
           asset.max.z > asset.min.z;
  }

  /// The point the model is turned about: the centre of its footprint at
  /// the height it rests on.
  Vec3 pivot(const EditorAsset& asset) {
    return {(asset.min.x + asset.max.x) * 0.5f,
            (asset.min.y + asset.max.y) * 0.5f, asset.min.z};
  }

  /// Sine and cosine of each Euler angle, in the order they are applied.
  struct EulerTrig {
    /// Sine of the X angle.
    float sx = 0.0f;
    /// Cosine of the X angle.
    float cx = 1.0f;
    /// Sine of the Y angle.
    float sy = 0.0f;
    /// Cosine of the Y angle.
    float cy = 1.0f;
    /// Sine of the Z angle.
    float sz = 0.0f;
    /// Cosine of the Z angle.
    float cz = 1.0f;
  };

  EulerTrig eulerTrig(const Vec3& degrees) {
    return {std::sin(degrees.x * DEGREES_TO_RADIANS),
            std::cos(degrees.x * DEGREES_TO_RADIANS),
            std::sin(degrees.y * DEGREES_TO_RADIANS),
            std::cos(degrees.y * DEGREES_TO_RADIANS),
            std::sin(degrees.z * DEGREES_TO_RADIANS),
            std::cos(degrees.z * DEGREES_TO_RADIANS)};
  }

  /// Rotation matrix for Euler degrees applied X, then Y, then Z.
  Mat4 eulerRotation(const Vec3& degrees) {
    const EulerTrig t = eulerTrig(degrees);
    Mat4 out{};
    out(0, 0) = t.cz * t.cy;
    out(0, 1) = t.cz * t.sy * t.sx - t.sz * t.cx;
    out(0, 2) = t.cz * t.sy * t.cx + t.sz * t.sx;
    out(1, 0) = t.sz * t.cy;
    out(1, 1) = t.sz * t.sy * t.sx + t.cz * t.cx;
    out(1, 2) = t.sz * t.sy * t.cx - t.cz * t.sx;
    out(2, 0) = -t.sy;
    out(2, 1) = t.cy * t.sx;
    out(2, 2) = t.cy * t.cx;
    out(3, 3) = 1.0f;
    return out;
  }

  /// Multiply the rotation part of @p m by a uniform scale.
  void scaleBasis(Mat4& m, float scale) {
    for (size_t col = 0; col < 3; ++col) {
      for (size_t row = 0; row < 3; ++row) {
        m(row, col) *= scale;
      }
    }
  }

  /// Set the translation that carries the model's pivot to @p target.
  void setPivotTranslation(Mat4& m, const Vec3& pivot_point,
                           const Vec3& target) {
    for (size_t row = 0; row < 3; ++row) {
      const float turned = m(row, 0) * pivot_point.x +
                           m(row, 1) * pivot_point.y +
                           m(row, 2) * pivot_point.z;
      m(row, 3) = (row == 0   ? target.x
                   : row == 1 ? target.y
                              : target.z) -
                  turned;
    }
  }

  /// Where a placement's pivot ends up in the world: the centre of its
  /// tile, at the placement's own height.
  Vec3 restingPoint(const EditorPlacement& placement) {
    return {placement.position.x + 0.5f, placement.position.y + 0.5f,
            placement.position.z};
  }

  /// The unit box standing on a placement's tile, used for an asset whose
  /// real bounds are not known.
  PlacementBounds unitTileBounds(const EditorPlacement& placement) {
    const Vec3 base = restingPoint(placement);
    // Scaled as the mesh would be, so a placement whose bounds are not
    // known yet is still picked and collided with at the size it will draw.
    const float half = 0.5f * placement.scale;
    return {{base.x - half, base.y - half, base.z},
            {base.x + half, base.y + half, base.z + placement.scale}};
  }

  /// Grow @p bounds to contain @p point.
  void enclose(PlacementBounds& bounds, const Vec3& point) {
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
  }

  /// One of the asset box's eight corners, chosen by the low three bits of
  /// @p corner.
  Vec3 assetCorner(const EditorAsset& asset, int corner) {
    return {(corner & 1) != 0 ? asset.max.x : asset.min.x,
            (corner & 2) != 0 ? asset.max.y : asset.min.y,
            (corner & 4) != 0 ? asset.max.z : asset.min.z};
  }

}  // namespace

Mat4 makePlacementTransform(const EditorAsset& asset,
                            const EditorPlacement& placement) {
  // Scale about the model's pivot, turn it there, then carry the pivot to
  // where the placement sits. Written out rather than multiplied so the
  // zero-rotation case stays the plain scale-and-offset it always was.
  Mat4 out = eulerRotation(placement.rotation);
  scaleBasis(out, footprintScale(asset) * placement.scale);
  setPivotTranslation(out, pivot(asset), restingPoint(placement));
  out(3, 3) = 1.0f;
  return out;
}

PlacementBounds placementWorldBounds(const EditorAsset& asset,
                                     const EditorPlacement& placement) {
  if (!hasMeasuredBounds(asset)) {
    return unitTileBounds(placement);
  }
  const Mat4 transform = makePlacementTransform(asset, placement);
  const Vec3 first = math::transformPoint(transform, assetCorner(asset, 0));
  PlacementBounds bounds{first, first};
  for (int corner = 1; corner < 8; ++corner) {
    enclose(bounds,
            math::transformPoint(transform, assetCorner(asset, corner)));
  }
  return bounds;
}

}  // namespace eng::editor
