#include "ui-widget-style.h"

#include <algorithm>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-panel.h>
#include <game/ui/ui-screen-view.h>
#include <game/ui/ui-text.h>
#include <utility>

namespace eng::game {

namespace {

  /// A new panel under @p parent in @p tree, filled @p fill; the panel.
  GuiPanel& addPanel(GuiWidgetTree& tree, GuiWidgetId parent, GuiColor fill) {
    auto& panel = *dynamic_cast<GuiPanel*>(
        tree.findWidget(tree.createWidget(GuiWidgetType::PANEL, parent)));
    panel.fill_color = fill;
    return panel;
  }

  /// Point @p widget's text — a label's or a button's — at @p text.
  void showText(GuiWidget& widget, std::string_view text) {
    if (auto* button = dynamic_cast<GuiButton*>(&widget)) {
      button->label = text;
    } else if (auto* label = dynamic_cast<GuiLabel*>(&widget)) {
      label->text = text;
    }
  }

}  // namespace

UiScreenView::UiScreenView(UiScreen screen, OnAction on_action)
  : screen_(std::move(screen)), on_action_(std::move(on_action)) {}

GuiWidgetId UiScreenView::build(GuiWidgetTree& tree, GuiWidgetId parent) {
  const bool menu = screen_.layer == UiScreenLayer::MENU;
  GuiPanel& cover = addPanel(tree, parent, menu ? UI_MENU_SCRIM : UI_CLEAR);
  anchorUiRoot(cover.tree_layout, screen_.anchor, screen_.inset);
  cover.z_index = menu ? 200 : 100;
  cover.debug_name = "ui:" + screen_.id;
  cover.pointer_through = !menu;
  overlay_ = cover.widget_id;
  buildNode(tree, overlay_, screen_.root);
  if (screen_.anchor == UiAnchor::FILL &&
      !tree.findWidget(overlay_)->children.empty()) {
    tree.findWidget(tree.findWidget(overlay_)->children.front())
        ->tree_layout.flex_grow = 1.0F;
  }
  (void)apply(tree, {});
  return overlay_;
}

GuiWidgetId UiScreenView::buildOne(GuiWidgetTree& tree, GuiWidgetId parent,
                                   const UiNode& node) {
  if (node.kind == UiNodeKind::LABEL) {
    return buildLabel(tree, parent, node);
  }
  if (node.kind == UiNodeKind::BUTTON) {
    return buildButton(tree, parent, node);
  }
  if (node.kind == UiNodeKind::BAR) {
    return buildBar(tree, parent, node);
  }
  GuiPanel& panel = addPanel(tree, parent, node.style.fill.value_or(UI_CLEAR));
  panel.corner_radius = node.style.radius;
  return panel.widget_id;
}

// NOLINTNEXTLINE(misc-no-recursion) -- a screen is a tree, bounded in depth
void UiScreenView::buildNode(GuiWidgetTree& tree, GuiWidgetId parent,
                             const UiNode& node) {
  const GuiWidgetId made = buildOne(tree, parent, node);
  GuiWidget& widget = *tree.findWidget(made);
  styleUiWidget(widget, node);
  widget.id = node.id;
  // A HUD takes no input: the pointer goes through it to the game.
  widget.pointer_through = screen_.layer == UiScreenLayer::HUD;
  for (const UiNode& child : node.children) {
    buildNode(tree, made, child);
  }
}

GuiWidgetId UiScreenView::buildLabel(GuiWidgetTree& tree, GuiWidgetId parent,
                                     const UiNode& node) {
  auto& label = *dynamic_cast<GuiLabel*>(
      tree.findWidget(tree.createWidget(GuiWidgetType::TEXT, parent)));
  label.color = node.style.color.value_or(UI_TEXT_COLOR);
  texts_.push_back({label.widget_id, node.text, {}});
  return label.widget_id;
}

GuiWidgetId UiScreenView::buildButton(GuiWidgetTree& tree, GuiWidgetId parent,
                                      const UiNode& node) {
  auto& button = *dynamic_cast<GuiButton*>(
      tree.findWidget(tree.createWidget(GuiWidgetType::BUTTON, parent)));
  button.state_styles = uiButtonLook(node.style);
  button.onClick([this, action = node.action](const GuiMouseEvent&) {
    on_action_(action);
  });
  buttons_.push_back({button.widget_id, node.id, node.action, texts_.size()});
  texts_.push_back({button.widget_id, node.text, {}});
  return button.widget_id;
}

GuiWidgetId UiScreenView::buildBar(GuiWidgetTree& tree, GuiWidgetId parent,
                                   const UiNode& node) {
  GuiPanel& track =
      addPanel(tree, parent, node.style.color.value_or(UI_BAR_EMPTY));
  track.corner_radius = node.style.radius;
  const GuiWidgetId fill =
      addPanel(tree, track.widget_id, node.style.fill.value_or(UI_BAR_FILL))
          .widget_id;
  const GuiWidgetId rest = addPanel(tree, track.widget_id, UI_CLEAR).widget_id;
  for (const GuiWidgetId part : {fill, rest}) {
    tree.findWidget(part)->tree_layout.flex_basis = 0.0F;
  }
  bars_.push_back({fill, rest, node.value, node.max});
  return track.widget_id;
}

bool UiScreenView::apply(GuiWidgetTree& tree, const UiValues& values) {
  bool changed = false;
  for (TextSlot& slot : texts_) {
    changed = fillText(tree, slot, values) || changed;
  }
  for (BarSlot& slot : bars_) {
    changed = fillBar(tree, slot, values) || changed;
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
    showText(*widget, slot.shown);
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

void UiScreenView::destroy(GuiWidgetTree& tree) {
  tree.destroyWidget(overlay_);
  overlay_ = GUI_WIDGET_ID_INVALID;
  texts_.clear();
  bars_.clear();
  buttons_.clear();
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

}  // namespace eng::game
