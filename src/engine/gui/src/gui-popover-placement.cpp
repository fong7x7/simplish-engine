#include <algorithm>
#include <engine/gui/gui-popover-placement.h>

namespace eng {

namespace {

  bool isVertical(GuiPopoverSide side) {
    return side == GuiPopoverSide::BELOW || side == GuiPopoverSide::ABOVE;
  }

  GuiPopoverSide opposite(GuiPopoverSide side) {
    switch (side) {
      case GuiPopoverSide::BELOW:
        return GuiPopoverSide::ABOVE;
      case GuiPopoverSide::ABOVE:
        return GuiPopoverSide::BELOW;
      case GuiPopoverSide::RIGHT:
        return GuiPopoverSide::LEFT;
      case GuiPopoverSide::LEFT:
        return GuiPopoverSide::RIGHT;
    }
    return side;
  }

  /// Room between the anchor and the viewport's edge on @p side.
  float roomOn(GuiPopoverSide side, const Rect& anchor, const Rect& view) {
    switch (side) {
      case GuiPopoverSide::BELOW:
        return view.y + view.h - (anchor.y + anchor.h);
      case GuiPopoverSide::ABOVE:
        return anchor.y - view.y;
      case GuiPopoverSide::RIGHT:
        return view.x + view.w - (anchor.x + anchor.w);
      case GuiPopoverSide::LEFT:
        return anchor.x - view.x;
    }
    return 0.0f;
  }

  /// Where along an axis @p length starts against an anchor from
  /// @p start, @p extent long, aligned by @p align.
  float alignAlong(Align align, float start, float extent, float length) {
    if (align == Align::CENTER) {
      return start + (extent - length) * 0.5f;
    }
    return align == Align::END ? start + extent - length : start;
  }

  /// The rect on @p side of @p anchor, before keeping it on screen.
  Rect onSide(GuiPopoverSide side, const Rect& anchor, LayoutSize size,
              const GuiPopoverPlacement& p) {
    if (isVertical(side)) {
      const float x = alignAlong(p.align, anchor.x, anchor.w, size.w);
      const float y = side == GuiPopoverSide::BELOW
                          ? anchor.y + anchor.h + p.gap
                          : anchor.y - p.gap - size.h;
      return {x, y, size.w, size.h};
    }
    const float y = alignAlong(p.align, anchor.y, anchor.h, size.h);
    const float x = side == GuiPopoverSide::RIGHT ? anchor.x + anchor.w + p.gap
                                                  : anchor.x - p.gap - size.w;
    return {x, y, size.w, size.h};
  }

  /// @p start slid so @p length stays inside [lo, hi], or at lo when it
  /// cannot.
  float keepInside(float start, float length, float lo, float hi) {
    return std::max(lo, std::min(start, hi - length));
  }

}  // namespace

Rect placePopover(const Rect& anchor, LayoutSize size, const Rect& viewport,
                  const GuiPopoverPlacement& placement) {
  const float need =
      (isVertical(placement.side) ? size.h : size.w) + placement.gap;
  GuiPopoverSide side = placement.side;
  const float room = roomOn(side, anchor, viewport) - placement.margin;
  const float other =
      roomOn(opposite(side), anchor, viewport) - placement.margin;
  if (room < need && other > room) {
    side = opposite(side);
  }
  Rect r = onSide(side, anchor, size, placement);
  const float m = placement.margin;
  r.x = keepInside(r.x, r.w, viewport.x + m, viewport.x + viewport.w - m);
  r.y = keepInside(r.y, r.h, viewport.y + m, viewport.y + viewport.h - m);
  return r;
}

}  // namespace eng
