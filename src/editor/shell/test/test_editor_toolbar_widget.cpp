#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-toolbar-widget.h>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <optional>

using Catch::Approx;
using namespace eng::editor;

namespace {

/// Build a tree with a root panel and an initialised toolbar under it.
/// No window and no GPU — the toolbar is pure widget-tree manipulation.
struct ToolbarFixture {
  eng::GuiWidgetTree tree;
  eng::GuiWidgetId root = eng::GUI_WIDGET_ID_INVALID;
  eng::GuiWidgetId toolbar_id = eng::GUI_WIDGET_ID_INVALID;

  ToolbarFixture() {
    root = tree.createWidget(eng::GuiWidgetType::PANEL,
                             eng::GUI_WIDGET_ID_INVALID);
    toolbar_id = tree.insertExternalWidget(
        std::make_unique<EditorToolbarWidget>(), root);
    bar()->init(tree);
  }

  EditorToolbarWidget* bar() {
    return dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id));
  }
};

constexpr eng::Rect BAR_RECT{0.0f, 28.0f, 1280.0f, TOOLBAR_HEIGHT};

}  // namespace

TEST_CASE("the toolbar creates one button per tool") {
  ToolbarFixture fx;
  // Buttons plus the project label and the status label.
  REQUIRE(fx.tree.childCount(fx.toolbar_id) == std::size(EDITOR_TOOLS) + 2);
}

TEST_CASE("the toolbar starts on the select tool") {
  ToolbarFixture fx;
  REQUIRE(fx.bar()->activeTool() == EditorTool::SELECT);
}

TEST_CASE("layout keeps every tool button inside the bar") {
  ToolbarFixture fx;
  fx.bar()->layout(fx.tree, BAR_RECT);

  size_t buttons_seen = 0;
  for (eng::GuiWidgetId child : fx.tree.findWidget(fx.toolbar_id)->children) {
    auto* button = dynamic_cast<eng::GuiButton*>(fx.tree.findWidget(child));
    if (button == nullptr) {
      continue;
    }
    ++buttons_seen;
    REQUIRE(button->rect.h > 0.0f);
    REQUIRE(button->rect.y >= BAR_RECT.y);
    REQUIRE(button->rect.y + button->rect.h <= BAR_RECT.y + BAR_RECT.h);
  }
  REQUIRE(buttons_seen == std::size(EDITOR_TOOLS));
}

TEST_CASE("tool buttons do not overlap") {
  ToolbarFixture fx;
  fx.bar()->layout(fx.tree, BAR_RECT);

  float previous_right = 0.0f;
  for (eng::GuiWidgetId child : fx.tree.findWidget(fx.toolbar_id)->children) {
    auto* button = dynamic_cast<eng::GuiButton*>(fx.tree.findWidget(child));
    if (button == nullptr) {
      continue;
    }
    REQUIRE(button->rect.x >= previous_right);
    previous_right = button->rect.x + button->rect.w;
  }
}

TEST_CASE("clicking a tool button selects it and reports the selection") {
  ToolbarFixture fx;
  std::optional<EditorTool> reported;
  fx.bar()->on_tool_selected = [&reported](EditorTool tool) {
    reported = tool;
  };
  fx.bar()->layout(fx.tree, BAR_RECT);

  // The second button is TILE_PAINT — EDITOR_TOOLS defines toolbar order.
  eng::GuiButton* tile_button = nullptr;
  size_t index = 0;
  for (eng::GuiWidgetId child : fx.tree.findWidget(fx.toolbar_id)->children) {
    auto* button = dynamic_cast<eng::GuiButton*>(fx.tree.findWidget(child));
    if (button == nullptr) {
      continue;
    }
    if (EDITOR_TOOLS[index] == EditorTool::TILE_PAINT) {
      tile_button = button;
      break;
    }
    ++index;
  }
  REQUIRE(tile_button != nullptr);

  eng::GuiMouseEvent click{};
  click.x = tile_button->rect.x + 1.0f;
  click.y = tile_button->rect.y + 1.0f;
  tile_button->handleClick(click);

  REQUIRE(reported.has_value());
  REQUIRE(*reported == EditorTool::TILE_PAINT);
  REQUIRE(fx.bar()->activeTool() == EditorTool::TILE_PAINT);
}

TEST_CASE("the active button is styled differently from the others") {
  ToolbarFixture fx;
  fx.bar()->setActiveTool(EditorTool::HEIGHT);
  fx.bar()->tick(fx.tree);

  size_t index = 0;
  bool checked_active = false;
  bool checked_inactive = false;
  for (eng::GuiWidgetId child : fx.tree.findWidget(fx.toolbar_id)->children) {
    auto* button = dynamic_cast<eng::GuiButton*>(fx.tree.findWidget(child));
    if (button == nullptr) {
      continue;
    }
    if (EDITOR_TOOLS[index] == EditorTool::HEIGHT) {
      REQUIRE(button->style.bg_color.r == eng::THEME_ACCENT.r);
      checked_active = true;
    } else {
      REQUIRE(button->style.bg_color.r == eng::THEME_BTN.r);
      checked_inactive = true;
    }
    ++index;
  }
  REQUIRE(checked_active);
  REQUIRE(checked_inactive);
}

TEST_CASE("label text tracks the strings the toolbar owns") {
  ToolbarFixture fx;
  fx.bar()->setProjectName("Transit Station");
  fx.bar()->setStatusText("zoom 150%   tile 4, 9");
  fx.bar()->tick(fx.tree);

  bool saw_project = false;
  bool saw_status = false;
  for (eng::GuiWidgetId child : fx.tree.findWidget(fx.toolbar_id)->children) {
    auto* label = dynamic_cast<eng::GuiLabel*>(fx.tree.findWidget(child));
    if (label == nullptr) {
      continue;
    }
    saw_project = saw_project || label->text == "Transit Station";
    saw_status = saw_status || label->text == "zoom 150%   tile 4, 9";
  }
  REQUIRE(saw_project);
  REQUIRE(saw_status);
}

TEST_CASE("shutdown removes every widget the toolbar created") {
  ToolbarFixture fx;
  REQUIRE(fx.tree.childCount(fx.toolbar_id) > 0);
  fx.bar()->shutdown(fx.tree);
  REQUIRE(fx.tree.childCount(fx.toolbar_id) == 0);
}

TEST_CASE("tick after shutdown is a no-op rather than a crash") {
  ToolbarFixture fx;
  fx.bar()->shutdown(fx.tree);
  fx.bar()->tick(fx.tree);
  SUCCEED();
}
