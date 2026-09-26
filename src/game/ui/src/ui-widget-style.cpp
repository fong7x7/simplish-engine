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

  /// How much lighter a button's face is under the pointer.
  constexpr float HOVER_LIGHTEN = 0.18F;
  /// White, which a face is lightened towards.
  constexpr GuiColor WHITE{255, 255, 255, 255};

  /// Set @p layout's sizes from @p s.
  void sizeLayout(LayoutStyle& layout, const UiNodeStyle& s) {
    layout.width = s.width;
    layout.height = s.height;
    layout.width_percent = s.width_percent;
    layout.height_percent = s.height_percent;
    layout.min_width = s.min_width;
    layout.min_height = s.min_height;
    layout.max_width = s.max_width;
    layout.max_height = s.max_height;
  }

}  // namespace

void styleUiWidget(GuiWidget& widget, const UiNode& node) {
  const UiNodeStyle& s = node.style;
  LayoutStyle& layout = widget.tree_layout;
  layout.direction = s.direction;
  layout.gap = s.gap;
  layout.padding = s.padding;
  layout.margin = s.margin;
  layout.margin_auto = s.margin_auto;
  sizeLayout(layout, s);
  layout.flex_grow =
      s.grow.value_or(node.kind == UiNodeKind::SPACER ? 1.0F : 0.0F);
  layout.flex_shrink = s.shrink.value_or(1.0F);
  layout.align_items = s.align_items;
  layout.justify_content = s.justify;
  layout.align_self = s.align_self;
  widget.opacity = s.opacity;
  applyKindDefaults(layout, node);
}

GuiStateStyles uiButtonLook(const UiNodeStyle& style, const GuiTheme& theme) {
  const GuiColor fill = style.fill.value_or(theme.palette.control);
  const GuiColor hover = GuiColor::lerp(fill, WHITE, HOVER_LIGHTEN);
  GuiStateStyles look = GuiStateStyles::uniform(
      {.fill = fill,
       .text = style.color.value_or(theme.palette.text),
       .radius = style.radius > 0.0F ? style.radius : UI_BUTTON_RADIUS});
  look.of(GuiWidgetState::HOVER).fill = hover;
  look.of(GuiWidgetState::FOCUSED).fill = hover;
  look.of(GuiWidgetState::SELECTED).fill = hover;
  look.of(GuiWidgetState::PRESSED).fill = GuiColor::lerp(fill, hover, 0.5F);
  look.of(GuiWidgetState::DISABLED).fill = GuiColor::applyOpacity(fill, 0.5F);
  return look;
}

std::optional<GuiStateStyles> uiPanelLook(const UiNodeStyle& style,
                                          const GuiTheme& theme) {
  if (style.border <= 0.0F && style.elevation == GuiElevation::NONE) {
    return std::nullopt;
  }
  return GuiStateStyles::uniform(
      {.fill = style.fill.value_or(UI_CLEAR),
       .text = style.color.value_or(theme.palette.text),
       .border = style.border_color.value_or(theme.palette.border),
       .border_width = style.border,
       .radius = style.radius,
       .elevation = style.elevation});
}

std::optional<GuiFont> uiTextFont(const UiNodeStyle& style, GuiTextRole role,
                                  const GuiTheme& theme) {
  if (style.text_size <= 0.0F && style.weight == 0) {
    return std::nullopt;
  }
  GuiFont font = theme.font(role);
  font.size = style.text_size > 0.0F ? style.text_size : font.size;
  font.weight = style.weight != 0 ? style.weight : font.weight;
  return font;
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
