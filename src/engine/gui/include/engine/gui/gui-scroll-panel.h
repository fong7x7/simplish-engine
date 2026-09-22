#pragma once

/// @file gui-scroll-panel.h
/// @brief A vertical list of widgets taller than the space it has.
/// @par Threading
/// Main thread only.

#include "gui-panel.h"
#include "scroll-state.h"

#include <optional>

namespace eng {

/// A panel that stacks its children down a column at their own heights and
/// scrolls them when they are taller than it is: a settings list, a
/// save-slot menu, a long dropdown of levels.
///
/// Children are laid out whenever the panel is — by `computeLayout`,
/// `arrangeWidget`, or itself after a scroll — at their `tree_layout.height`
/// (or `row_height`), `tree_layout.gap` apart, inside `tree_layout.padding`,
/// and are drawn clipped to it. The wheel scrolls it. Focus moving to a
/// child scrolls just far enough to show it, and a pad or arrow direction
/// with nothing focusable that way scrolls by `nav_step`, so text below the
/// last button can still be read.
///
/// Inserted with `insertExternalWidget`; the tree's own SCROLL_CONTAINER
/// nodes stay plain panels, which the markdown renderer lays out itself.
class GuiScrollPanel : public GuiPanel {
public:
  /// An empty panel, scrolled to the top.
  GuiScrollPanel();

  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// The panel, and a thumb down its right edge when it can scroll.
  void render(const GuiDrawContext& ctx) const override;

  /// Stack the children down @p available, moved up by the scroll.
  void arrangeChildren(GuiWidgetTree& tree, const Rect& available) override;

  /// Scroll by the wheel; true if it moved.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// Scroll just far enough that @p child shows; true if it moved.
  bool revealChild(const Rect& child) override;

  /// UP and DOWN scroll by `nav_step`; true if it moved.
  bool scrollByNav(GuiNavCommand command) override;

  /// Lay the children out again where the scroll has put them.
  void arrangeAfterScroll(GuiWidgetTree& tree) override;

  /// The inside of the padding.
  [[nodiscard]] std::optional<Rect> childClipRect() const override;

  /// How far the children are scrolled up, in pixels: 0 at the top.
  [[nodiscard]] float scrollOffset() const { return scroll_.offset_y; }

  /// The furthest they can scroll: 0 when they all fit.
  [[nodiscard]] float maxScroll() const;

  /// Scroll to @p offset, held to [0, `maxScroll`]. Takes effect on the
  /// children at the next layout.
  void setScrollOffset(float offset);

  /// Height of a child that does not set `tree_layout.height`.
  float row_height = 32.0f;
  /// How far one pad or arrow step scrolls, in pixels.
  float nav_step = 48.0f;
  /// How far one notch of the wheel scrolls, in pixels.
  float wheel_step = 40.0f;

private:
  /// The rect inside the padding that children show through.
  [[nodiscard]] Rect viewport() const;

  /// Offset, content height and velocity.
  ScrollState scroll_{};
  /// Height of the rect inside the padding, as of the last layout.
  float viewport_h_ = 0.0f;
};

}  // namespace eng
