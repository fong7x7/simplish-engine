#include "ui-view-widgets.h"
#include "ui-widget-style.h"

#include <engine/gui/gui-button.h>
#include <engine/gui/gui-checkbox.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-toggle.h>
#include <game/ui/ui-screen-view.h>
#include <utility>

namespace eng::game {

namespace {

  /// Every state a look has, to set one thing in all of them.
  constexpr GuiWidgetState STATES[] = {
      GuiWidgetState::NORMAL,   GuiWidgetState::HOVER,
      GuiWidgetState::PRESSED,  GuiWidgetState::FOCUSED,
      GuiWidgetState::DISABLED, GuiWidgetState::SELECTED};

  /// A new panel under @p parent in @p tree, filled @p fill; the panel.
  GuiPanel& addPanel(GuiWidgetTree& tree, GuiWidgetId parent, GuiColor fill) {
    auto& panel = *dynamic_cast<GuiPanel*>(
        tree.findWidget(tree.createWidget(GuiWidgetType::PANEL, parent)));
    panel.fill_color = fill;
    return panel;
  }

  /// @p theme's look for a button of @p style's variant, at its radius
  /// when it gives one.
  GuiStateStyles themedButtonLook(const UiNodeStyle& style,
                                  const GuiTheme& theme) {
    GuiStateStyles look = theme.button(style.variant);
    if (style.radius > 0.0F) {
      for (const GuiWidgetState state : STATES) {
        look.of(state).radius = style.radius;
      }
    }
    return look;
  }

}  // namespace

UiScreenView::UiScreenView(UiScreen screen, OnAction on_action,
                           std::shared_ptr<const GuiTheme> theme)
  : screen_(std::move(screen)), on_action_(std::move(on_action)),
    theme_(theme != nullptr ? std::move(theme)
                            : std::make_shared<GuiTheme>(GuiTheme::dark())) {}

GuiWidgetId UiScreenView::build(GuiWidgetTree& tree, GuiWidgetId parent) {
  const bool menu = screen_.layer == UiScreenLayer::MENU;
  GuiPanel& cover =
      addPanel(tree, parent, menu ? theme_->palette.scrim : UI_CLEAR);
  anchorUiRoot(cover.tree_layout, screen_.anchor, screen_.inset);
  cover.z_index = menu ? 200 : 100;
  cover.debug_name = "ui:" + screen_.id;
  cover.pointer_through = !menu;
  cover.subtree_theme = theme_;
  overlay_ = cover.widget_id;
  buildNode(tree, overlay_, screen_.root);
  if (screen_.anchor == UiAnchor::FILL) {
    tree.findWidget(tree.findWidget(overlay_)->children.front())
        ->tree_layout.flex_grow = 1.0F;
  }
  (void)apply(tree, {});
  return overlay_;
}

GuiWidgetId UiScreenView::buildOne(GuiWidgetTree& tree, GuiWidgetId parent,
                                   const UiNode& node) {
  switch (node.kind) {
    case UiNodeKind::LABEL:
      return buildLabel(tree, parent, node);
    case UiNodeKind::BUTTON:
      return buildButton(tree, parent, node);
    case UiNodeKind::BAR:
      return buildBar(tree, parent, node);
    case UiNodeKind::CHECKBOX:
    case UiNodeKind::TOGGLE:
      return buildCheck(tree, parent, node);
    default:
      return buildPanel(tree, parent, node);
  }
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
  track(made, node);
  for (const UiNode& child : node.children) {
    buildNode(tree, made, child);
  }
}

void UiScreenView::track(GuiWidgetId widget, const UiNode& node) {
  const UiBindings& b = node.bind;
  if (!b.visible.empty() || !b.disabled.empty() || !b.selected.empty() ||
      !b.checked.empty()) {
    flags_.push_back({widget, b});
  }
  if (!node.id.empty()) {
    named_.push_back({widget, node.id, node.kind});
  }
}

GuiWidgetId UiScreenView::buildPanel(GuiWidgetTree& tree, GuiWidgetId parent,
                                     const UiNode& node) {
  GuiPanel& panel = addPanel(tree, parent, node.style.fill.value_or(UI_CLEAR));
  panel.corner_radius = node.style.radius;
  panel.state_styles = uiPanelLook(node.style, *theme_);
  return panel.widget_id;
}

GuiWidgetId UiScreenView::buildLabel(GuiWidgetTree& tree, GuiWidgetId parent,
                                     const UiNode& node) {
  auto& label = *dynamic_cast<GuiLabel*>(
      tree.findWidget(tree.createWidget(GuiWidgetType::TEXT, parent)));
  const UiNodeStyle& s = node.style;
  label.color = s.color;
  label.role = s.role.value_or(GuiTextRole::BODY);
  label.font = uiTextFont(s, label.role, *theme_);
  label.wrap = s.wrap;
  label.align = s.text_align;
  texts_.push_back({label.widget_id, node.text, {}});
  return label.widget_id;
}

GuiWidgetId UiScreenView::buildButton(GuiWidgetTree& tree, GuiWidgetId parent,
                                      const UiNode& node) {
  auto& button = *dynamic_cast<GuiButton*>(
      tree.findWidget(tree.createWidget(GuiWidgetType::BUTTON, parent)));
  const UiNodeStyle& s = node.style;
  button.variant = s.variant;
  button.role = s.role.value_or(GuiTextRole::LABEL);
  button.state_styles = s.fill || s.color ? uiButtonLook(s, *theme_)
                                          : themedButtonLook(s, *theme_);
  button.onClick([this, action = node.action](const GuiMouseEvent&) {
    on_action_(action);
  });
  addChooser(button.widget_id, node);
  return button.widget_id;
}

GuiWidgetId UiScreenView::buildCheck(GuiWidgetTree& tree, GuiWidgetId parent,
                                     const UiNode& node) {
  const GuiWidgetId made = tree.insertExternalWidget(
      node.kind == UiNodeKind::TOGGLE
          ? std::unique_ptr<GuiWidget>(std::make_unique<GuiToggle>())
          : std::make_unique<GuiCheckbox>(),
      parent);
  // Bound, it shows the value, never its own guess: a press flips it
  // back and asks, and the logic's answer is what it shows.
  onUiCheckChange(
      *tree.findWidget(made), [this, &tree, made, action = node.action,
                               bound = !node.bind.checked.empty()](bool now) {
        if (bound) {
          setUiChecked(*tree.findWidget(made),
                       now ? GuiCheckState::UNCHECKED : GuiCheckState::CHECKED);
        }
        on_action_(action);
      });
  addChooser(made, node);
  return made;
}

void UiScreenView::addChooser(GuiWidgetId widget, const UiNode& node) {
  buttons_.push_back({widget, node.id, node.action, texts_.size()});
  texts_.push_back({widget, node.text, {}});
}

GuiWidgetId UiScreenView::buildBar(GuiWidgetTree& tree, GuiWidgetId parent,
                                   const UiNode& node) {
  GuiPanel& track = addPanel(
      tree, parent, node.style.color.value_or(theme_->palette.surface_sunken));
  track.corner_radius = node.style.radius;
  const GuiWidgetId fill =
      addPanel(tree, track.widget_id,
               node.style.fill.value_or(theme_->palette.success))
          .widget_id;
  const GuiWidgetId rest = addPanel(tree, track.widget_id, UI_CLEAR).widget_id;
  for (const GuiWidgetId part : {fill, rest}) {
    tree.findWidget(part)->tree_layout.flex_basis = 0.0F;
  }
  bars_.push_back({fill, rest, node.value, node.max});
  return track.widget_id;
}

}  // namespace eng::game
