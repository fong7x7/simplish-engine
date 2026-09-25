#include "ui-widget-style.h"

namespace eng::game {

namespace {

  /// Where an anchor puts a screen's root: up and down, and across.
  /// `STRETCH` both ways is `FILL`.
  struct AnchorPlace {
    /// Up and down.
    Align down = Align::CENTER;
    /// Across.
    Align across = Align::CENTER;
  };

  /// Each anchor's place, in `UiAnchor` order.
  constexpr AnchorPlace PLACES[] = {
      {Align::CENTER, Align::CENTER}, {Align::START, Align::CENTER},
      {Align::END, Align::CENTER},    {Align::CENTER, Align::START},
      {Align::CENTER, Align::END},    {Align::START, Align::START},
      {Align::START, Align::END},     {Align::END, Align::START},
      {Align::END, Align::END},       {Align::STRETCH, Align::STRETCH}};

}  // namespace

void styleUiWidget(GuiWidget& widget, const UiNode& node) {
  const UiNodeStyle& s = node.style;
  LayoutStyle& layout = widget.tree_layout;
  layout.direction = s.direction;
  layout.gap = s.gap;
  layout.padding = s.padding;
  layout.margin = s.margin;
  layout.width = s.width;
  layout.height = s.height;
  layout.min_width = s.min_width;
  layout.min_height = s.min_height;
  layout.flex_grow =
      s.grow.value_or(node.kind == UiNodeKind::SPACER ? 1.0F : 0.0F);
  layout.align_items = s.align_items;
  layout.justify_content = s.justify;
  layout.align_self = s.align_self;
  if (node.kind == UiNodeKind::BAR) {
    // A bar's two parts share its width: filled, then empty.
    layout.direction = FlexDirection::ROW;
    layout.height = s.height < 0.0F ? UI_BAR_HEIGHT : s.height;
  }
  widget.override_style = true;
}

void anchorUiRoot(LayoutStyle& layout, UiAnchor anchor, float inset) {
  const AnchorPlace place = PLACES[static_cast<size_t>(anchor)];
  layout.position = PositionMode::ABSOLUTE;
  layout.abs_right = 0.0F;
  layout.abs_bottom = 0.0F;
  layout.padding = {inset, inset, inset, inset};
  layout.justify_content =
      place.down == Align::STRETCH ? Align::START : place.down;
  layout.align_items = place.across;
}

}  // namespace eng::game
