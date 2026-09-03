#include <editor/shell/iso-view-matrix.h>

namespace eng::editor {

namespace {

  /// Screen position, in layout pixels, that world (0, 0, 0) projects to.
  IsoPoint projectedOrigin(const IsoView& view, const Rect& viewport) {
    const float cx = viewport.x + viewport.w * 0.5f;
    const float cy = viewport.y + viewport.h * 0.5f;
    return {cx - view.focus.x * view.zoom, cy - view.focus.y * view.zoom};
  }

  /// Fill the row that maps world X to clip X.
  void setClipXRow(Mat4& out, const IsoView& view,
                   const IsoViewTarget& target) {
    const float to_ndc = 2.0f / target.layout_width;
    const IsoPoint origin = projectedOrigin(view, target.viewport);
    out(0, 0) = ISO_TILE_WIDTH * view.zoom * to_ndc;
    out(0, 3) = origin.x * to_ndc - 1.0f;
  }

  /// Fill the row that maps world Y and Z to clip Y. NDC y points up while
  /// layout y points down, so both terms flip.
  void setClipYRow(Mat4& out, const IsoView& view,
                   const IsoViewTarget& target) {
    const float to_ndc = 2.0f / target.layout_height;
    const IsoPoint origin = projectedOrigin(view, target.viewport);
    out(1, 1) = -ISO_TILE_DEPTH * view.zoom * to_ndc;
    out(1, 2) = ISO_TILE_RISE * view.zoom * to_ndc;
    out(1, 3) = 1.0f - origin.y * to_ndc;
  }

  /// Fill the row that measures depth along the projection ray.
  void setClipDepthRow(Mat4& out) {
    // Normalise the ray components to tiles, then to half the depth range,
    // so clip z sits at 0.5 for anything on the ground at the origin.
    const float per_tile = 1.0f / (ISO_RAY_LENGTH * ISO_DEPTH_RANGE);
    out(2, 1) = -ISO_TILE_RISE * per_tile;
    out(2, 2) = -ISO_TILE_DEPTH * per_tile;
    out(2, 3) = 0.5f;
  }

}  // namespace

Mat4 makeIsoViewProjection(const IsoView& view, const IsoViewTarget& target) {
  Mat4 out{};
  setClipXRow(out, view, target);
  setClipYRow(out, view, target);
  setClipDepthRow(out);
  out(3, 3) = 1.0f;
  return out;
}

}  // namespace eng::editor
