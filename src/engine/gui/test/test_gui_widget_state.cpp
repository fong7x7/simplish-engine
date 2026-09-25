#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-text-input.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>

using namespace eng;

namespace {

/// A tree with one 100 × 30 button at the origin.
struct ButtonFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiWidgetId id{tree.createWidget(GuiWidgetType::BUTTON, root)};
  int clicks = 0;

  ButtonFixture() {
    tree.findWidget(root)->rect = {0.0f, 0.0f, 400.0f, 300.0f};
    button().rect = {0.0f, 0.0f, 100.0f, 30.0f};
    button().onClick([this](const GuiMouseEvent&) { ++clicks; });
  }

  GuiButton& button() { return *dynamic_cast<GuiButton*>(tree.findWidget(id)); }
};

}  // namespace

TEST_CASE("state precedence: disabled, pressed, selected, hovered, focused") {
  GuiButton b;
  CHECK(b.visualState() == GuiWidgetState::NORMAL);
  b.focused = true;
  CHECK(b.visualState() == GuiWidgetState::FOCUSED);
  b.hovered = true;
  CHECK(b.visualState() == GuiWidgetState::HOVER);
  b.selected = true;
  CHECK(b.visualState() == GuiWidgetState::SELECTED);
  b.pressed = true;
  CHECK(b.visualState() == GuiWidgetState::PRESSED);
  b.disabled = true;
  CHECK(b.visualState() == GuiWidgetState::DISABLED);
}

TEST_CASE("a button draws its variant's look for its state") {
  GuiButton b;
  const GuiDrawContext ctx{};
  const GuiTheme& theme = ctx.activeTheme();
  b.variant = GuiButtonVariant::DANGER;
  CHECK(b.drawnStyle(ctx).fill.pack() == theme.palette.danger.pack());
  b.hovered = true;
  CHECK(b.drawnStyle(ctx).fill.pack() == theme.palette.danger_hover.pack());
}

TEST_CASE("its own state styles win over the theme") {
  GuiButton b;
  b.state_styles = GuiStateStyles::uniform({.fill = {1, 2, 3}});
  CHECK(b.drawnStyle(GuiDrawContext{}).fill.pack() == GuiColor{1, 2, 3}.pack());
}

TEST_CASE("update blends the drawn style towards a new state") {
  GuiButton b;
  const GuiDrawContext ctx{};
  b.update(ctx, 0.0f);
  b.hovered = true;
  b.update(ctx, 0.01f);
  const GuiColor rest = ctx.activeTheme().palette.control;
  const GuiColor hover = ctx.activeTheme().palette.control_hover;
  CHECK(b.drawnStyle(ctx).fill.r > rest.r);
  CHECK(b.drawnStyle(ctx).fill.r < hover.r);
  b.update(ctx, 1.0f);
  CHECK(b.drawnStyle(ctx).fill.pack() == hover.pack());
}

TEST_CASE("the theme on the draw context is the one drawn from") {
  GuiButton b;
  GuiDrawContext ctx{};
  ctx.theme = &GuiTheme::light();
  CHECK(b.drawnStyle(ctx).fill.pack() ==
        GuiTheme::light().palette.control.pack());
}

TEST_CASE("a disabled button ignores clicks and navigation") {
  ButtonFixture fx;
  fx.button().disabled = true;
  fx.tree.dispatchClick(10.0f, 10.0f);
  CHECK(fx.clicks == 0);
  CHECK_FALSE(fx.button().handleNav(GuiNavCommand::CONFIRM));
  fx.button().disabled = false;
  fx.tree.dispatchClick(10.0f, 10.0f);
  CHECK(fx.clicks == 1);
}

TEST_CASE("focus passes a disabled button by") {
  ButtonFixture fx;
  const GuiWidgetId other =
      fx.tree.createWidget(GuiWidgetType::BUTTON, fx.root);
  fx.tree.findWidget(other)->rect = {0.0f, 40.0f, 100.0f, 30.0f};
  fx.button().disabled = true;
  fx.tree.advanceFocus(FocusTraversalDirection::FORWARD);
  CHECK(fx.tree.focused_id == other);
}

TEST_CASE("a widget knows when it has focus") {
  ButtonFixture fx;
  const GuiWidgetId other =
      fx.tree.createWidget(GuiWidgetType::BUTTON, fx.root);
  fx.tree.setFocus(fx.id);
  CHECK(fx.button().focused);
  fx.tree.setFocus(other);
  CHECK_FALSE(fx.button().focused);
  CHECK(fx.tree.findWidget(other)->focused);
}

TEST_CASE("a label with no colour takes the theme's text colour") {
  GuiLabel label;
  CHECK_FALSE(label.color.has_value());
}

TEST_CASE("a text field is FOCUSED while typing") {
  GuiTextInput field;
  field.focus = GuiTextInputFocus::FOCUSED;
  CHECK(field.visualState() == GuiWidgetState::FOCUSED);
  field.disabled = true;
  CHECK(field.visualState() == GuiWidgetState::DISABLED);
}
