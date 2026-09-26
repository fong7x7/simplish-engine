#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-dropdown.h>
#include <engine/gui/gui-menu-bar.h>
#include <memory>

using namespace eng;

namespace {

/// A root with a menu bar across its top.
struct MenuFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiMenuBar* bar = nullptr;
  int saved = 0;

  MenuFixture() {
    const GuiWidgetId id =
        tree.insertExternalWidget(std::make_unique<GuiMenuBar>(), root);
    bar = dynamic_cast<GuiMenuBar*>(tree.findWidget(id));
    bar->tree_layout.height = 26.0f;
    bar->menus = {
        {"File", {{.label = "Save", .on_select = [this] { ++saved; }}}},
        {"Edit", {{.label = "Undo"}}}};
    bar->build(tree);
    tree.computeLayout({0, 0, 600, 400});
  }

  GuiDropdown& menu(size_t i) {
    const GuiWidgetId layer = tree.overlayLayer();
    std::vector<GuiDropdown*> menus;
    for (const GuiWidgetId id : tree.findWidget(layer)->children) {
      if (auto* d = dynamic_cast<GuiDropdown*>(tree.findWidget(id))) {
        menus.push_back(d);
      }
    }
    return *menus.at(i);
  }
};

}  // namespace

TEST_CASE("a title opens its menu under it; a row runs and closes it") {
  MenuFixture fx;
  fx.bar->open(fx.tree, 0);
  CHECK(fx.bar->openIndex() == 0);
  GuiDropdown& file = fx.menu(0);
  CHECK(file.visible);
  CHECK(file.rect.y >= 26.0f);
  file.selectItem(0);
  CHECK(fx.saved == 1);
  CHECK(fx.bar->openIndex() == -1);
  CHECK_FALSE(file.visible);
}

TEST_CASE("with a menu open, the pointer on another title opens that one") {
  MenuFixture fx;
  fx.bar->open(fx.tree, 0);
  const GuiWidget& edit = *fx.tree.findWidget(fx.bar->children.at(1));
  fx.tree.dispatchMouseMove({.type = GuiMouseEventType::MOVE,
                             .x = edit.rect.x + 2,
                             .y = edit.rect.y + 2});
  CHECK(fx.bar->openIndex() == 1);
  CHECK(fx.menu(1).visible);
  CHECK_FALSE(fx.menu(0).visible);
}
