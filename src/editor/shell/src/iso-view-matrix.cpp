#include <cmath>
#include <editor/shell/iso-view-matrix.h>

namespace eng::editor {

namespace {

  /// Screen position, in layout pixels, that world (0, 0, 0) projects to.
  IsoPoint projectedOrigin(const IsoView& view, const Rect& viewport) {
    const float cx = viewport.x + viewport.w * 0.5f;
    const float cy = viewport.y + viewport.h * 0.5f;
    return {cx - view.focus.x * view.zoom, cy - view.focus.y * view.zoom};
  }

  /// Fill the row that maps world X and Y to clip X.
  void setClipXRow(Mat4& out, const IsoView& view,
                   const IsoViewTarget& target) {
    const float to_ndc = 2.0f / target.layout_width;
    const IsoPoint origin = projectedOrigin(view, target.viewport);
    out(0, 0) = view.axes.x_across * view.zoom * to_ndc;
    out(0, 1) = view.axes.y_across * view.zoom * to_ndc;
    out(0, 3) = origin.x * to_ndc - 1.0f;
  }

  /// Fill the row that maps all three world axes to clip Y. NDC y points up
  /// while layout y points down, so every term flips.
  void setClipYRow(Mat4& out, const IsoView& view,
                   const IsoViewTarget& target) {
    const float to_ndc = 2.0f / target.layout_height;
    const IsoPoint origin = projectedOrigin(view, target.viewport);
    out(1, 0) = -view.axes.x_down * view.zoom * to_ndc;
    out(1, 1) = -view.axes.y_down * view.zoom * to_ndc;
    out(1, 2) = view.axes.z_up * view.zoom * to_ndc;
    out(1, 3) = 1.0f - origin.y * to_ndc;
  }

  /// Fill the row that measures depth along the projection ray.
  void setClipDepthRow(Mat4& out, const IsoAxes& axes) {
    // Normalise the ray to tiles, then to half the depth range, so clip z
    // sits at 0.5 for anything on the ground at the origin.
    const WorldPoint ray = isoProjectionRay(axes);
    const float per_tile = 1.0f / (isoRayLength(axes) * ISO_DEPTH_RANGE);
    out(2, 0) = -ray.x * per_tile;
    out(2, 1) = -ray.y * per_tile;
    out(2, 2) = -ray.z * per_tile;
    out(2, 3) = 0.5f;
  }

}  // namespace

float isoRayLength(const IsoAxes& axes) {
  const WorldPoint ray = isoProjectionRay(axes);
  return std::sqrt(ray.x * ray.x + ray.y * ray.y + ray.z * ray.z);
}

Mat4 makeIsoViewProjection(const IsoView& view, const IsoViewTarget& target) {
  Mat4 out{};
  setClipXRow(out, view, target);
  setClipYRow(out, view, target);
  setClipDepthRow(out, view.axes);
  out(3, 3) = 1.0f;
  return out;
}

}  // namespace eng::editor
