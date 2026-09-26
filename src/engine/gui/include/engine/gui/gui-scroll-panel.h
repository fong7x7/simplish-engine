#pragma once

/// @file gui-scroll-panel.h
/// @brief A list of widgets longer than the space it has.
/// @par Threading
/// Main thread only.

#include "gui-panel.h"
#include "gui-scroll-axis.h"

#include <optional>

namespace eng {

/// A panel that stacks its children along one axis at their own sizes and
/// scrolls them when they run past its end: a settings list, a save-slot
/// menu, a strip of character cards.
///
/// Children are laid out whenever the panel is — by `computeLayout`,
/// `arrangeWidget`, or itself after a scroll — by flexbox along the axis,
/// in a box as long as they need: at their own or measured size, else
/// `item_size`, with their margins, `tree_layout.gap` apart, inside
/// `tree_layout.padding`, aligned across by `align_items`, and drawn
/// clipped to it. The wheel scrolls it, and so does a pad's right stick
/// (`GuiWidgetTree::scrollFocusBy`). Focus moving to a child scrolls just
/// far enough to show it, and a direction along the axis with nothing
/// focusable that way scrolls by `nav_step`, so text past the last button
/// can still be read.
///
/// Inserted with `insertExternalWidget`; the tree's own SCROLL_CONTAINER
/// nodes stay plain panels, which the markdown renderer lays out itself.
class GuiScrollPanel : public GuiPanel {
public:
  /// An empty panel, scrolled to the start.
  GuiScrollPanel();

  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// The panel, and a thumb along its far edge when it can scroll.
  void render(const GuiDrawContext& ctx) const override;

  /// Stack the children along @p available, moved back by the scroll.
  void arrangeChildren(GuiWidgetTree& tree, const Rect& available) override;

  /// Scroll by the wheel — along either axis; true if it moved.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// Scroll by (@p dx, @p dy) pixels, the part along the axis; true if it
  /// moved.
  bool scrollBy(float dx, float dy) override;

  /// Scroll just far enough that @p child shows; true if it moved.
  bool revealChild(const Rect& child) override;

  /// The two directions along the axis scroll by `nav_step`; true if it
  /// moved.
  bool scrollByNav(GuiNavCommand command) override;

  /// Lay the children out again where the scroll has put them.
  void arrangeAfterScroll(GuiWidgetTree& tree) override;

  /// The inside of the padding.
  [[nodiscard]] std::optional<Rect> childClipRect() const override;

  /// How far the children are scrolled, in pixels: 0 at the start.
  [[nodiscard]] float scrollOffset() const { return offset_; }

  /// The furthest they can scroll: 0 when they all fit.
  [[nodiscard]] float maxScroll() const;

  /// Scroll to @p offset, held to [0, `maxScroll`]. Takes effect on the
  /// children at the next layout.
  void setScrollOffset(float offset);

  /// Which way the children stack and scroll.
  GuiScrollAxis axis = GuiScrollAxis::VERTICAL;
  /// Size along the axis of a child that does not set its own.
  float item_size = 32.0f;
  /// How far one pad or arrow step scrolls, in pixels.
  float nav_step = 48.0f;
  /// How far one notch of the wheel scrolls, in pixels.
  float wheel_step = 40.0f;

  /// Its content's scrolled top-left: a child scrolled along has not
  /// moved, so does not glide.
  [[nodiscard]] DrawPos contentOrigin() const override;

private:
  /// Give each child with no size of its own along the axis, and nothing
  /// measured, `item_size`.
  void sizeUnmeasured(GuiWidgetTree& tree) const;

  /// The box its children lie in: as long as they need along the axis,
  /// and slid back by the scroll.
  [[nodiscard]] Rect scrolledBox() const;

  /// The rect inside the padding that children show through.
  [[nodiscard]] Rect viewport() const;

  /// The thumb's rect, for a panel that can scroll.
  [[nodiscard]] Rect thumbRect() const;

  /// How far the children are scrolled along the axis.
  float offset_ = 0.0f;
  /// Their total length along it, gaps included.
  float content_ = 0.0f;
  /// The viewport's length along it, as of the last layout.
  float viewport_ = 0.0f;
};

}  // namespace eng
