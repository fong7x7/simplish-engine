#include "engine/gui/gui-dropdown.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"

#include <utility>

namespace eng {

std::unique_ptr<GuiWidget> GuiDropdown::clone() const {
  return std::make_unique<GuiDropdown>(*this);
}

namespace {

  /// Horizontal text padding inside dropdown items.
  constexpr int ITEM_TEXT_PAD = 12;
  /// Vertical text padding inside dropdown items.
  constexpr int ITEM_TEXT_VPAD = 5;
  /// Inset for hover highlight (avoid covering border).
  constexpr int HOVER_INSET = 1;
  /// Alpha multiplier applied to the text colour for disabled rows so
  /// they read as greyed-out without needing a separate style colour.
  constexpr float DISABLED_TEXT_OPACITY = 0.4F;
  /// Alpha multiplier for separator lines.
  constexpr float SEPARATOR_OPACITY = 0.3F;
  /// Thickness of a separator line in logical pixels.
  constexpr float SEPARATOR_THICKNESS = 1.0F;
  /// Gap between the right edge of the menu and a shortcut hint.
  constexpr int SHORTCUT_PAD = 12;
  /// Side of the square drawn in a checked row's left gutter.
  constexpr float CHECK_MARK_SIZE = 5.0F;
  /// Left inset of that square, which keeps it clear of the label without
  /// moving the label: rows must not shift as a setting is switched.
  constexpr float CHECK_MARK_INSET = 3.5F;

}  // namespace

GuiDropdown::ResolvedStyle GuiDropdown::resolveStyle() const {
  const bool use_shared = hasSharedStyle();
  auto bg = use_shared ? ui_style->dropdown_bg : style.bg_color;
  auto text = use_shared ? ui_style->dropdown_text : style.text_color;
  auto hover = use_shared ? ui_style->dropdown_hover : style.hover_color;
  return {GuiColor::applyOpacity(bg, opacity),
          GuiColor::applyOpacity(text, opacity),
          GuiColor::applyOpacity(hover, opacity),
          use_shared ? ui_style->dropdown_width : style.width,
          use_shared ? ui_style->dropdown_item_height : style.item_height};
}

void GuiDropdown::renderBackground(const GuiDrawContext& ctx,
                                   const ResolvedStyle& rs) const {
  const auto wf = static_cast<float>(rs.width);
  const auto ihf = static_cast<float>(rs.item_height);
  const float h = static_cast<float>(static_cast<int>(items.size())) * ihf;
  Rect bg{rect.x, rect.y, wf, h};
  ctx.drawRoundedRect(bg, rs.bg, corner_radius);
  if (border_width > 0.0f) {
    ctx.drawRoundedBorderRect({bg, border_color, corner_radius, border_width});
  } else {
    ctx.drawBorderRect(bg, rs.bg);
  }
}

void GuiDropdown::renderHoverHighlight(const GuiDrawContext& ctx,
                                       const ResolvedStyle& rs,
                                       float iy) const {
  const auto wf = static_cast<float>(rs.width);
  const auto ihf = static_cast<float>(rs.item_height);
  Rect hr{rect.x + static_cast<float>(HOVER_INSET), iy,
          wf - 2.0f * static_cast<float>(HOVER_INSET), ihf};
  ctx.drawFilledRect(hr, rs.hover);
}

void GuiDropdown::renderSeparator(const GuiDrawContext& ctx,
                                  const ResolvedStyle& rs, float iy) const {
  const auto wf = static_cast<float>(rs.width);
  const auto ihf = static_cast<float>(rs.item_height);
  const auto pad = static_cast<float>(ITEM_TEXT_PAD);
  Rect line{rect.x + pad, iy + (ihf * 0.5F) - (SEPARATOR_THICKNESS * 0.5F),
            wf - 2.0F * pad, SEPARATOR_THICKNESS};
  ctx.drawFilledRect(line, GuiColor::applyOpacity(rs.text, SEPARATOR_OPACITY));
}

void GuiDropdown::renderItemText(const GuiDrawContext& ctx,
                                 const RowParams& row) const {
  const auto wf = static_cast<float>(row.rs.width);
  const auto ihf = static_cast<float>(row.rs.item_height);
  const auto color =
      row.item.enabled
          ? row.rs.text
          : GuiColor::applyOpacity(row.rs.text, DISABLED_TEXT_OPACITY);
  Rect line_rect{rect.x, row.iy, wf, ihf};
  const DrawPos label_pos =
      drawPosInset(line_rect, static_cast<float>(ITEM_TEXT_PAD),
                   static_cast<float>(ITEM_TEXT_VPAD));
  ctx.drawText(color, label_pos, row.item.label);
  if (row.item.checked) {
    renderCheckMark(ctx, row.rs, row.iy);
  }
  renderShortcut(ctx, row, label_pos.y);
}

void GuiDropdown::renderCheckMark(const GuiDrawContext& ctx,
                                  const ResolvedStyle& rs, float iy) const {
  const auto ihf = static_cast<float>(rs.item_height);
  Rect mark{rect.x + CHECK_MARK_INSET, iy + (ihf - CHECK_MARK_SIZE) * 0.5F,
            CHECK_MARK_SIZE, CHECK_MARK_SIZE};
  ctx.drawFilledRect(mark, rs.text);
}

void GuiDropdown::renderShortcut(const GuiDrawContext& ctx,
                                 const RowParams& row, float text_y) const {
  if (row.item.shortcut.empty()) {
    return;
  }
  // Right-align the accelerator against the menu's right edge, which is set
  // by the style width rather than by `rect.w`.
  const float shortcut_w = ctx.measureText(row.item.shortcut);
  const float x = rect.x + static_cast<float>(row.rs.width) -
                  static_cast<float>(SHORTCUT_PAD) - shortcut_w;
  ctx.drawText(GuiColor::applyOpacity(row.rs.text, DISABLED_TEXT_OPACITY),
               {x, text_y}, row.item.shortcut);
}

void GuiDropdown::renderItems(const GuiDrawContext& ctx,
                              const ResolvedStyle& rs) const {
  const auto ihf = static_cast<float>(rs.item_height);
  for (int i = 0; std::cmp_less(i, items.size()); ++i) {
    const auto& item = items[static_cast<size_t>(i)];
    float iy = rect.y + static_cast<float>(i) * ihf;
    if (item.separator) {
      renderSeparator(ctx, rs, iy);
      continue;
    }
    if (i == hovered_item && item.enabled) {
      renderHoverHighlight(ctx, rs, iy);
    }
    renderItemText(ctx, {rs, item, iy});
  }
}

void GuiDropdown::render(const GuiDrawContext& ctx) const {
  auto rs = resolveStyle();
  renderBackground(ctx, rs);
  renderItems(ctx, rs);
}

int GuiDropdown::hitTestItem(float mx, float my) const {
  if (!visible) {
    return -1;
  }
  int count = static_cast<int>(items.size());
  auto h = static_cast<float>(count * style.item_height);
  const auto sw = static_cast<float>(style.width);
  if (mx < rect.x || mx >= rect.x + sw) {
    return -1;
  }
  if (my < rect.y || my >= rect.y + h) {
    return -1;
  }
  return static_cast<int>((my - rect.y) /
                          static_cast<float>(style.item_height));
}

void GuiDropdown::selectItem(int index) {
  if (std::cmp_less(index, 0) || std::cmp_greater_equal(index, items.size())) {
    return;
  }
  auto& item = items[static_cast<size_t>(index)];
  if (!item.enabled || item.separator) {
    return;
  }
  if (item.on_select) {
    item.on_select();
  }
}

}  // namespace eng
