#include "ui-view-widgets.h"

#include <algorithm>
#include <game/ui/ui-screen-view.h>
#include <game/ui/ui-text.h>
#include <utility>

namespace eng::game {

bool UiScreenView::apply(GuiWidgetTree& tree, const UiValues& values) {
  bool changed = false;
  for (TextSlot& slot : texts_) {
    changed = fillText(tree, slot, values) || changed;
  }
  for (BarSlot& slot : bars_) {
    changed = fillBar(tree, slot, values) || changed;
  }
  for (const FlagSlot& slot : flags_) {
    changed = fillFlags(tree, slot, values) || changed;
  }
  return changed;
}

bool UiScreenView::fillText(GuiWidgetTree& tree, TextSlot& slot,
                            const UiValues& values) {
  std::string shown = fillUiText(slot.pattern, values);
  const bool changed = shown != slot.shown;
  slot.shown = std::move(shown);
  // Always, not only when changed: the slot may have moved since.
  if (GuiWidget* widget = tree.findWidget(slot.widget)) {
    showUiText(*widget, slot.shown);
  }
  return changed;
}

bool UiScreenView::fillBar(GuiWidgetTree& tree, BarSlot& slot,
                           const UiValues& values) {
  const float full = uiNumber(values, slot.max).value_or(0.0F);
  const float share =
      full > 0.0F
          ? std::clamp(uiNumber(values, slot.value).value_or(0.0F) / full, 0.0F,
                       1.0F)
          : 0.0F;
  if (share == slot.shown) {
    return false;
  }
  slot.shown = share;
  tree.findWidget(slot.fill)->tree_layout.flex_grow = share;
  tree.findWidget(slot.rest)->tree_layout.flex_grow = 1.0F - share;
  return true;
}

bool UiScreenView::fillFlags(GuiWidgetTree& tree, const FlagSlot& slot,
                             const UiValues& values) {
  GuiWidget* widget = tree.findWidget(slot.widget);
  if (widget == nullptr) {
    return false;
  }
  const bool was_visible = widget->visible;
  widget->visible = uiFlag(values, slot.bind.visible).value_or(was_visible);
  widget->disabled =
      uiFlag(values, slot.bind.disabled).value_or(widget->disabled);
  widget->selected =
      uiFlag(values, slot.bind.selected).value_or(widget->selected);
  if (const auto checked = uiFlag(values, slot.bind.checked)) {
    setUiChecked(*widget,
                 *checked ? GuiCheckState::CHECKED : GuiCheckState::UNCHECKED);
  }
  return widget->visible != was_visible;
}

void UiScreenView::destroy(GuiWidgetTree& tree) {
  tree.destroyWidget(overlay_);
  overlay_ = GUI_WIDGET_ID_INVALID;
  texts_.clear();
  bars_.clear();
  buttons_.clear();
  flags_.clear();
  named_.clear();
}

GuiWidgetId UiScreenView::firstButton() const {
  return buttons_.empty() ? GUI_WIDGET_ID_INVALID : buttons_.front().widget;
}

std::vector<UiButtonInfo>
UiScreenView::buttons(const GuiWidgetTree& tree) const {
  std::vector<UiButtonInfo> out;
  for (const ButtonSlot& slot : buttons_) {
    const GuiWidget* widget = tree.findWidget(slot.widget);
    out.push_back({slot.id, slot.action, texts_[slot.text].shown,
                   widget != nullptr ? widget->rect : Rect{}});
  }
  return out;
}

std::vector<UiNodeInfo> UiScreenView::nodes(const GuiWidgetTree& tree) const {
  std::vector<UiNodeInfo> out;
  for (const NamedSlot& slot : named_) {
    if (const GuiWidget* widget = tree.findWidget(slot.widget)) {
      out.push_back({slot.id, slot.kind, widget->rect, widget->visible,
                     widget->disabled, widget->selected});
    }
  }
  return out;
}

}  // namespace eng::game
