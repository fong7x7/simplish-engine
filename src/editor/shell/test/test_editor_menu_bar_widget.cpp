#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-menu-bar-widget.h>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-dropdown.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using Catch::Approx;
using namespace eng::editor;

namespace {

constexpr eng::Rect WINDOW{0.0f, 0.0f, 1280.0f, 800.0f};
constexpr eng::Rect BAR_RECT{0.0f, 28.0f, 1280.0f, MENU_BAR_HEIGHT};

/// Menu indices, in the order `MENU_SPECS` declares them.
constexpr size_t FILE_MENU = 0;
constexpr size_t EDIT_MENU = 1;
constexpr size_t VIEW_MENU = 2;

/// A tree holding a window-sized root and an initialised, laid-out menu bar.
/// No window and no GPU — this is pure widget-tree manipulation.
struct MenuFixture {
  eng::GuiWidgetTree tree;
  eng::GuiWidgetId root = eng::GUI_WIDGET_ID_INVALID;
  eng::GuiWidgetId bar_id = eng::GUI_WIDGET_ID_INVALID;

  MenuFixture() {
    root = tree.createWidget(eng::GuiWidgetType::PANEL,
                             eng::GUI_WIDGET_ID_INVALID);
    tree.findWidget(root)->rect = WINDOW;
    bar_id = tree.insertExternalWidget(std::make_unique<EditorMenuBarWidget>(),
                                       root);
    bar()->init(tree);
    bar()->layout(tree, BAR_RECT, WINDOW);
  }

  EditorMenuBarWidget* bar() {
    return dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(bar_id));
  }

  eng::GuiDropdown* menu(size_t index) {
    return dynamic_cast<eng::GuiDropdown*>(
        tree.findWidget(bar()->dropdownId(index)));
  }
};

/// Click through the tree the way the platform layer does, then let the bar
/// apply the request on its next tick.
void clickAt(MenuFixture& fx, float x, float y) {
  fx.tree.dispatchClick(x, y);
  fx.bar()->tick(fx.tree);
}

/// Click the centre of a menu title.
void clickTitle(MenuFixture& fx, size_t index) {
  const eng::Rect& r = fx.tree.findWidget(fx.bar()->titleButtonId(index))->rect;
  clickAt(fx, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
}

/// Click the centre of one row of an open menu.
void clickRow(MenuFixture& fx, size_t menu_index, int row) {
  const eng::GuiDropdown* menu = fx.menu(menu_index);
  const auto height = static_cast<float>(menu->style.item_height);
  clickAt(fx, menu->rect.x + 10.0f,
          menu->rect.y + (static_cast<float>(row) + 0.5f) * height);
}

/// Index of the first row with the given label, or -1.
int rowWithLabel(const eng::GuiDropdown& menu, std::string_view label) {
  for (size_t i = 0; i < menu.items.size(); ++i) {
    if (menu.items[i].label == label) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

/// Whether the Edit menu row labelled @p label is live.
bool editRowEnabled(MenuFixture& fx, std::string_view label) {
  const eng::GuiDropdown& edit = *fx.menu(EDIT_MENU);
  return edit.items[static_cast<size_t>(rowWithLabel(edit, label))].enabled;
}

/// A history holding one applied placement, over @p document.
EditorActionHistory historyWithOnePlacement(EditorDocument& document) {
  EditorActionHistory history;
  performEditorAction(history, document,
                      {.kind = EditorActionKind::PLACE_ASSET,
                       .index = 0,
                       .placement = {0, {0.0f, 0.0f}}});
  return history;
}

}  // namespace

TEST_CASE("the bar creates one dropdown per title, all hidden") {
  MenuFixture fx;
  REQUIRE(fx.bar()->menuCount() == 4);
  REQUIRE(fx.bar()->openMenuIndex() == -1);
  for (size_t i = 0; i < fx.bar()->menuCount(); ++i) {
    REQUIRE(fx.menu(i) != nullptr);
    REQUIRE_FALSE(fx.menu(i)->visible);
  }
}

TEST_CASE("dropdowns are siblings of the bar, not children") {
  MenuFixture fx;
  // Hit testing never descends into a child outside its parent's rect, and a
  // menu hangs below the bar. Parenting them to the bar would make every row
  // unclickable.
  for (size_t i = 0; i < fx.bar()->menuCount(); ++i) {
    REQUIRE(fx.tree.findWidget(fx.bar()->dropdownId(i))->parent_id == fx.root);
  }
}

TEST_CASE("a dropdown is placed under its title and sized to its rows") {
  MenuFixture fx;
  const eng::GuiDropdown* menu = fx.menu(FILE_MENU);
  const eng::Rect& title =
      fx.tree.findWidget(fx.bar()->titleButtonId(FILE_MENU))->rect;

  REQUIRE(menu->rect.x == Approx(title.x));
  REQUIRE(menu->rect.y == Approx(BAR_RECT.y + BAR_RECT.h));
  REQUIRE(menu->rect.w == Approx(static_cast<float>(menu->style.width)));
  REQUIRE(menu->rect.h == Approx(static_cast<float>(
                              menu->items.size() *
                              static_cast<size_t>(menu->style.item_height))));
}

TEST_CASE("clicking a title opens its menu and clicking it again closes it") {
  MenuFixture fx;

  clickTitle(fx, FILE_MENU);
  REQUIRE(fx.bar()->openMenuIndex() == static_cast<int>(FILE_MENU));
  REQUIRE(fx.menu(FILE_MENU)->visible);

  clickTitle(fx, FILE_MENU);
  REQUIRE(fx.bar()->openMenuIndex() == -1);
  REQUIRE_FALSE(fx.menu(FILE_MENU)->visible);
}

TEST_CASE("only one menu is open at a time") {
  MenuFixture fx;
  fx.bar()->openMenu(fx.tree, static_cast<int>(FILE_MENU));
  fx.bar()->openMenu(fx.tree, static_cast<int>(VIEW_MENU));

  REQUIRE_FALSE(fx.menu(FILE_MENU)->visible);
  REQUIRE(fx.menu(VIEW_MENU)->visible);
}

TEST_CASE("an out-of-range menu index closes everything") {
  MenuFixture fx;
  fx.bar()->openMenu(fx.tree, static_cast<int>(FILE_MENU));
  fx.bar()->openMenu(fx.tree, 99);

  REQUIRE(fx.bar()->openMenuIndex() == -1);
  REQUIRE_FALSE(fx.menu(FILE_MENU)->visible);
}

TEST_CASE("choosing a row raises the command and closes the menu") {
  MenuFixture fx;
  std::vector<EditorMenuCommand> raised;
  fx.bar()->on_command = [&raised](EditorMenuCommand c) {
    raised.push_back(c);
  };

  clickTitle(fx, VIEW_MENU);
  clickRow(fx, VIEW_MENU, rowWithLabel(*fx.menu(VIEW_MENU), "Zoom In"));

  REQUIRE(raised.size() == 1);
  REQUIRE(raised[0] == EditorMenuCommand::ZOOM_IN);
  REQUIRE(fx.bar()->openMenuIndex() == -1);
}

TEST_CASE("a disabled row raises nothing") {
  MenuFixture fx;
  std::vector<EditorMenuCommand> raised;
  fx.bar()->on_command = [&raised](EditorMenuCommand c) {
    raised.push_back(c);
  };

  clickTitle(fx, FILE_MENU);
  clickRow(fx, FILE_MENU, rowWithLabel(*fx.menu(FILE_MENU), "Save"));

  REQUIRE(raised.empty());
}

TEST_CASE("clicking outside an open menu closes it") {
  MenuFixture fx;
  clickTitle(fx, FILE_MENU);
  REQUIRE(fx.bar()->openMenuIndex() == static_cast<int>(FILE_MENU));

  // Far below the menu, over what would be the viewport.
  clickAt(fx, 900.0f, 600.0f);
  REQUIRE(fx.bar()->openMenuIndex() == -1);
}

TEST_CASE("the scrim is only present while a menu is open") {
  MenuFixture fx;
  const eng::GuiWidget* scrim = nullptr;
  for (eng::GuiWidgetId child : fx.tree.findWidget(fx.root)->children) {
    if (fx.tree.findWidget(child)->debug_name == "editor-menu-scrim") {
      scrim = fx.tree.findWidget(child);
    }
  }
  REQUIRE(scrim != nullptr);
  REQUIRE_FALSE(scrim->visible);

  fx.bar()->openMenu(fx.tree, static_cast<int>(FILE_MENU));
  REQUIRE(scrim->visible);
  REQUIRE(scrim->rect.w == Approx(WINDOW.w));
}

TEST_CASE("moving onto another title while open switches menus") {
  MenuFixture fx;
  fx.bar()->openMenu(fx.tree, static_cast<int>(FILE_MENU));

  const eng::Rect& view_title =
      fx.tree.findWidget(fx.bar()->titleButtonId(VIEW_MENU))->rect;
  fx.tree.updateHover(view_title.x + view_title.w * 0.5f,
                      view_title.y + view_title.h * 0.5f);
  fx.bar()->tick(fx.tree);

  REQUIRE(fx.bar()->openMenuIndex() == static_cast<int>(VIEW_MENU));
}

TEST_CASE("hovering a title with every menu closed opens nothing") {
  MenuFixture fx;
  const eng::Rect& title =
      fx.tree.findWidget(fx.bar()->titleButtonId(FILE_MENU))->rect;
  fx.tree.updateHover(title.x + title.w * 0.5f, title.y + title.h * 0.5f);
  fx.bar()->tick(fx.tree);

  REQUIRE(fx.bar()->openMenuIndex() == -1);
}

TEST_CASE("unimplemented commands are listed but disabled") {
  MenuFixture fx;
  const eng::GuiDropdown& file = *fx.menu(FILE_MENU);
  REQUIRE_FALSE(
      file.items[static_cast<size_t>(rowWithLabel(file, "Save"))].enabled);
  REQUIRE(file.items[static_cast<size_t>(rowWithLabel(file, "Exit"))].enabled);
}

TEST_CASE("the project commands that have a dialog behind them are enabled") {
  MenuFixture fx;
  const eng::GuiDropdown& file = *fx.menu(FILE_MENU);
  // Both open an OS dialog, so neither needs a project already loaded.
  REQUIRE(file.items[static_cast<size_t>(rowWithLabel(file, "New Project..."))]
              .enabled);
  REQUIRE(file.items[static_cast<size_t>(rowWithLabel(file, "Open Project..."))]
              .enabled);
}

TEST_CASE("Close Project is disabled until a project is open") {
  MenuFixture fx;
  auto closeRow = [&fx]() {
    const eng::GuiDropdown& file = *fx.menu(FILE_MENU);
    return file.items[static_cast<size_t>(rowWithLabel(file, "Close Project"))]
        .enabled;
  };
  REQUIRE_FALSE(closeRow());

  fx.bar()->setProjectPresence(EditorProjectPresence::OPEN);
  fx.bar()->tick(fx.tree);
  REQUIRE(closeRow());
}

TEST_CASE("the View menu offers both projections") {
  MenuFixture fx;
  const eng::GuiDropdown& view = *fx.menu(VIEW_MENU);
  REQUIRE(rowWithLabel(view, "Dimetric View") >= 0);
  REQUIRE(rowWithLabel(view, "Isometric View") >= 0);
}

TEST_CASE("the projection rows need a project to record the choice in") {
  MenuFixture fx;
  auto isometricRow = [&fx]() {
    const eng::GuiDropdown& view = *fx.menu(VIEW_MENU);
    return view
        .items[static_cast<size_t>(rowWithLabel(view, "Isometric View"))];
  };
  REQUIRE_FALSE(isometricRow().enabled);

  fx.bar()->setProjectPresence(EditorProjectPresence::OPEN);
  fx.bar()->tick(fx.tree);
  REQUIRE(isometricRow().enabled);
}

TEST_CASE("exactly one projection row is marked, and it is the live one") {
  MenuFixture fx;
  auto marked = [&fx](std::string_view label) {
    const eng::GuiDropdown& view = *fx.menu(VIEW_MENU);
    return view.items[static_cast<size_t>(rowWithLabel(view, label))].checked;
  };
  // A project that has never been switched is dimetric, so that is the row
  // that carries the mark before anything is chosen.
  REQUIRE(marked("Dimetric View"));
  REQUIRE_FALSE(marked("Isometric View"));

  fx.bar()->setProjection(ProjectProjection::ISOMETRIC);
  fx.bar()->tick(fx.tree);
  REQUIRE(marked("Isometric View"));
  REQUIRE_FALSE(marked("Dimetric View"));
}

TEST_CASE("choosing a projection raises its command") {
  MenuFixture fx;
  fx.bar()->setProjectPresence(EditorProjectPresence::OPEN);
  fx.bar()->tick(fx.tree);
  std::vector<EditorMenuCommand> raised;
  fx.bar()->on_command = [&raised](EditorMenuCommand c) {
    raised.push_back(c);
  };

  clickTitle(fx, VIEW_MENU);
  clickRow(fx, VIEW_MENU, rowWithLabel(*fx.menu(VIEW_MENU), "Isometric View"));

  REQUIRE(raised.size() == 1);
  REQUIRE(raised[0] == EditorMenuCommand::SET_VIEW_ISOMETRIC);
}

TEST_CASE("Undo is disabled until an action has been taken") {
  MenuFixture fx;
  REQUIRE_FALSE(editRowEnabled(fx, "Undo"));

  EditorDocument document;
  const EditorActionHistory history = historyWithOnePlacement(document);
  fx.bar()->setHistory(history);
  fx.bar()->tick(fx.tree);

  REQUIRE(editRowEnabled(fx, "Undo"));
}

TEST_CASE("Redo is disabled until an action has been undone") {
  MenuFixture fx;
  EditorDocument document;
  EditorActionHistory history = historyWithOnePlacement(document);
  fx.bar()->setHistory(history);
  fx.bar()->tick(fx.tree);
  REQUIRE_FALSE(editRowEnabled(fx, "Redo"));

  REQUIRE(undoEditorAction(history, document));
  fx.bar()->setHistory(history);
  fx.bar()->tick(fx.tree);

  REQUIRE(editRowEnabled(fx, "Redo"));
  REQUIRE_FALSE(editRowEnabled(fx, "Undo"));
}

TEST_CASE("recent projects become rows in the File menu") {
  MenuFixture fx;
  RecentProjectsList recent;
  recent.entries.push_back({"/tmp/alpha", "Alpha", "2026-08-27T00:00:00Z"});
  recent.entries.push_back({"/tmp/beta", "Beta", "2026-08-26T00:00:00Z"});
  fx.bar()->setRecentProjects(recent);
  fx.bar()->tick(fx.tree);

  const eng::GuiDropdown& file = *fx.menu(FILE_MENU);
  REQUIRE(rowWithLabel(file, "Alpha") > rowWithLabel(file, "Open Project..."));
  REQUIRE(rowWithLabel(file, "Beta") > rowWithLabel(file, "Alpha"));
}

TEST_CASE("choosing a recent project reports its path") {
  MenuFixture fx;
  RecentProjectsList recent;
  recent.entries.push_back({"/tmp/alpha", "Alpha", "2026-08-27T00:00:00Z"});
  fx.bar()->setRecentProjects(recent);
  fx.bar()->tick(fx.tree);

  std::string opened;
  fx.bar()->on_open_recent = [&opened](std::string_view p) {
    opened = p;
  };
  clickTitle(fx, FILE_MENU);
  clickRow(fx, FILE_MENU, rowWithLabel(*fx.menu(FILE_MENU), "Alpha"));

  REQUIRE(opened == "/tmp/alpha");
}

TEST_CASE("an empty recent list adds no rows and no separator") {
  MenuFixture fx;
  const size_t rows_without_recent = fx.menu(FILE_MENU)->items.size();

  fx.bar()->setRecentProjects(RecentProjectsList{});
  fx.bar()->tick(fx.tree);

  REQUIRE(fx.menu(FILE_MENU)->items.size() == rows_without_recent);
}

TEST_CASE("only commands with a working key show a shortcut") {
  MenuFixture fx;
  const eng::GuiDropdown& view = *fx.menu(VIEW_MENU);
  const eng::GuiDropdown& file = *fx.menu(FILE_MENU);

  REQUIRE(
      view.items[static_cast<size_t>(rowWithLabel(view, "Zoom In"))].shortcut ==
      "=");
  REQUIRE(file.items[static_cast<size_t>(rowWithLabel(file, "Save"))]
              .shortcut.empty());
}

TEST_CASE("the undo keys are hinted with this platform's modifier") {
  MenuFixture fx;
  const eng::GuiDropdown& edit = *fx.menu(EDIT_MENU);
  const std::string_view undo =
      edit.items[static_cast<size_t>(rowWithLabel(edit, "Undo"))].shortcut;
  const std::string_view redo =
      edit.items[static_cast<size_t>(rowWithLabel(edit, "Redo"))].shortcut;

  // Both modifiers work at the handler; only the hint text is per-platform,
  // so the test pins the shape rather than one platform's spelling.
  REQUIRE(undo.ends_with("Z"));
  REQUIRE_FALSE(undo.contains("Shift"));
  REQUIRE(redo.ends_with("Shift+Z"));
  REQUIRE(redo.starts_with(undo.substr(0, undo.size() - 1)));
}

TEST_CASE("shutdown removes every widget the bar created") {
  MenuFixture fx;
  const size_t before = fx.tree.widget_nodes.size();
  const size_t owned =
      fx.tree.childCount(fx.bar_id) + fx.bar()->menuCount() + 1;

  fx.bar()->shutdown(fx.tree);

  REQUIRE(fx.tree.widget_nodes.size() == before - owned);
  REQUIRE(fx.bar()->menuCount() == 0);
}
