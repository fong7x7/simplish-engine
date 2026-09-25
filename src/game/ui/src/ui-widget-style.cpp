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

  /// Whether @p edges are all zero: none were given.
  bool noEdges(const Edges& edges) {
    return edges.top == 0.0F && edges.right == 0.0F && edges.bottom == 0.0F &&
           edges.left == 0.0F;
  }

  /// What a button and a bar take unless the node says otherwise: a
  /// button's padding round its text; a bar's row of two parts, its
  /// height, and — with no width — the whole width of its line.
  void applyKindDefaults(LayoutStyle& layout, const UiNode& node) {
    const UiNodeStyle& s = node.style;
    if (node.kind == UiNodeKind::BUTTON && noEdges(s.padding)) {
      layout.padding = UI_BUTTON_PADDING;
    }
    if (node.kind != UiNodeKind::BAR) {
      return;
    }
    layout.direction = FlexDirection::ROW;
    layout.height = s.height < 0.0F ? UI_BAR_HEIGHT : s.height;
    if (s.width < 0.0F && s.align_self == Align::AUTO) {
      layout.align_self = Align::STRETCH;
    }
  }

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
  applyKindDefaults(layout, node);
}

GuiStateStyles uiButtonLook(const UiNodeStyle& style) {
  const GuiColor fill = style.fill.value_or(UI_BUTTON_FILL);
  GuiStateStyles look = GuiStateStyles::uniform(
      {.fill = fill,
       .text = style.color.value_or(UI_TEXT_COLOR),
       .radius = style.radius > 0.0F ? style.radius : UI_BUTTON_RADIUS});
  look.of(GuiWidgetState::HOVER).fill = UI_BUTTON_HOVER;
  look.of(GuiWidgetState::SELECTED).fill = UI_BUTTON_HOVER;
  look.of(GuiWidgetState::PRESSED).fill =
      GuiColor::lerp(fill, UI_BUTTON_HOVER, 0.5F);
  look.of(GuiWidgetState::DISABLED).fill = GuiColor::applyOpacity(fill, 0.5F);
  return look;
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
