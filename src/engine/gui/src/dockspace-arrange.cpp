/// @file dockspace-arrange.cpp
/// @brief Pure-function region arithmetic for `GuiDockspaceWidget`. See
/// dockspace-arrange.h for the public contract and the Phase 1 leaf
/// (docs/technical-approaches/engine/gui/dockspace.md §2.2) for the
/// corner-precedence rules.

#include "engine/gui/dockspace-arrange.h"

#include "engine/gui/gui-widget-id.h"

#include <algorithm>
#include <cstddef>

namespace eng {

/// Sub-pixel tolerance for `rectsOverlap`. An overlap smaller than this
/// on either axis is treated as touching, not overlapping — prevents
/// 1-ULP float drift between adjacent region edges from firing the
/// debug invariant or failing property tests. Deliberately sub-pixel:
/// rendering will never show an overlap below this threshold.
constexpr float OVERLAP_EPS_PX = 1.0e-3f;

namespace {

  /// Reserved pixel size for a region, clamped to non-negative. Empty
  /// regions (no child assigned) contribute zero regardless of `size_px`.
  float reservedSize(const GuiDockLayout& layout, DockEdge edge) {
    const auto& region = layout.regions[static_cast<size_t>(edge)];
    if (region.child == GUI_WIDGET_ID_INVALID) {
      return 0.0f;
    }
    return std::max(0.0f, region.size_px);
  }

  /// Vertical band between the top and bottom edge regions; consumed by
  /// LEFT, RIGHT, and CENTRE for their height.
  struct MidBand {
    /// Y coordinate of the top of the band.
    float y = 0.0f;
    /// Height of the band in pixels (>= 0).
    float h = 0.0f;
  };

  /// TOP region rect: spans full viewport width; height clamped to viewport.
  Rect computeTopRect(const GuiDockLayout& layout, const Rect& viewport) {
    float sz = std::min(reservedSize(layout, DockEdge::TOP), viewport.h);
    return {viewport.x, viewport.y, viewport.w, sz};
  }

  /// BOTTOM region rect: spans full viewport width; height clamped to the
  /// space remaining after the TOP region has been reserved.
  Rect computeBottomRect(const GuiDockLayout& layout, const Rect& viewport,
                         const Rect& top) {
    float remaining = std::max(0.0f, viewport.h - top.h);
    float sz = std::min(reservedSize(layout, DockEdge::BOTTOM), remaining);
    return {viewport.x, viewport.y + viewport.h - sz, viewport.w, sz};
  }

  /// Mid-band computed from TOP and BOTTOM rects. Height is derived from
  /// `bottom.y - (top.y + top.h)` rather than
  /// `viewport.h - top.h - bottom.h` so LEFT/RIGHT/CENTRE's bottom edge
  /// exactly matches BOTTOM's y (avoids 1-ULP float overlaps).
  MidBand computeMidBand(const Rect& top, const Rect& bottom) {
    float y = top.y + top.h;
    return {y, std::max(0.0f, bottom.y - y)};
  }

  /// LEFT region rect: width clamped to viewport width; height is the mid-band.
  Rect computeLeftRect(const GuiDockLayout& layout, const Rect& viewport,
                       const MidBand& band) {
    float sz = std::min(reservedSize(layout, DockEdge::LEFT), viewport.w);
    return {viewport.x, band.y, sz, band.h};
  }

  /// RIGHT region rect: width clamped to space remaining after LEFT has
  /// been reserved; height is the mid-band.
  Rect computeRightRect(const GuiDockLayout& layout, const Rect& viewport,
                        const MidBand& band, float left_w) {
    float remaining = std::max(0.0f, viewport.w - left_w);
    float sz = std::min(reservedSize(layout, DockEdge::RIGHT), remaining);
    return {viewport.x + viewport.w - sz, band.y, sz, band.h};
  }

  /// CENTRE region rect: interior remainder between LEFT's right edge and
  /// RIGHT's left edge; height is the mid-band. Derived from `right.x`
  /// rather than `viewport.w - left_w - right_w` so CENTRE's right edge
  /// exactly matches RIGHT's x — avoiding 1-ULP floating-point overlaps
  /// between adjacent regions.
  Rect computeCentreRect(const Rect& viewport, const MidBand& band,
                         float left_w, const Rect& right) {
    float centre_x = viewport.x + left_w;
    float mid_w = std::max(0.0f, right.x - centre_x);
    return {centre_x, band.y, mid_w, band.h};
  }

}  // namespace

bool computeDockRegions(const GuiDockLayout& layout, const Rect& viewport,
                        std::array<Rect, DOCK_EDGE_COUNT>& output) {
  Rect top = computeTopRect(layout, viewport);
  Rect bot = computeBottomRect(layout, viewport, top);
  MidBand band = computeMidBand(top, bot);
  Rect left = computeLeftRect(layout, viewport, band);
  Rect right = computeRightRect(layout, viewport, band, left.w);
  Rect centre = computeCentreRect(viewport, band, left.w, right);
  output[static_cast<size_t>(DockEdge::TOP)] = top;
  output[static_cast<size_t>(DockEdge::BOTTOM)] = bot;
  output[static_cast<size_t>(DockEdge::LEFT)] = left;
  output[static_cast<size_t>(DockEdge::RIGHT)] = right;
  output[static_cast<size_t>(DockEdge::CENTRE)] = centre;
  return centre.w > 0.0f && centre.h > 0.0f;
}

bool rectsOverlap(const Rect& a, const Rect& b) {
  if (a.w <= 0.0f || a.h <= 0.0f || b.w <= 0.0f || b.h <= 0.0f) {
    return false;
  }
  // Intersection-area predicate against OVERLAP_EPS_PX tolerance.
  float x0 = std::max(a.x, b.x);
  float y0 = std::max(a.y, b.y);
  float x1 = std::min(a.x + a.w, b.x + b.w);
  float y1 = std::min(a.y + a.h, b.y + b.h);
  return (x1 - x0) > OVERLAP_EPS_PX && (y1 - y0) > OVERLAP_EPS_PX;
}

}  // namespace eng
