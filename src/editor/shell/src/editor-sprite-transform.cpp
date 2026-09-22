#include <editor/shell/editor-sprite-transform.h>
#include <editor/shell/iso-projection.h>

namespace eng::editor {

namespace {

  /// @p point as a vector, since the projection answers in world points.
  Vec3 asVector(const WorldPoint& point) {
    return {point.x, point.y, point.z};
  }

  /// Write @p axis into column @p column of @p out's basis.
  void setBasisColumn(Mat4& out, size_t column, const Vec3& axis) {
    out(0, column) = axis.x;
    out(1, column) = axis.y;
    out(2, column) = axis.z;
  }

  /// @p axis scaled to @p length.
  Vec3 scaled(const Vec3& axis, float length) {
    return {axis.x * length, axis.y * length, axis.z * length};
  }

}  // namespace

float editorSpriteWidth(const IsoAxes& axes, const EditorSprite& sprite,
                        Vec2 frame_pixels) {
  const float across = isoAcrossPixels(axes);
  // A frame with no height, or a projection that draws nothing sideways,
  // has no proportion to keep: the billboard stands square.
  if (frame_pixels.y <= 0.0f || across <= 0.0f) {
    return sprite.height;
  }
  return sprite.height * (frame_pixels.x / frame_pixels.y) *
         (axes.z_up / across);
}

Mat4 makeSpriteTransform(const IsoAxes& axes, const EditorSprite& sprite,
                         float width) {
  const Vec3 right = Vec3::normalize(asVector(isoScreenRight(axes)));
  const Vec3 toward = Vec3::normalize(asVector(isoProjectionRay(axes)));
  Mat4 out{};
  setBasisColumn(out, 0, scaled(right, width));
  setBasisColumn(out, 1, toward);
  setBasisColumn(out, 2, {0.0f, 0.0f, sprite.height});
  setBasisColumn(out, 3, asVector(sprite.position));
  out(3, 3) = 1.0f;
  return out;
}

}  // namespace eng::editor
