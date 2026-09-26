#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-widgets-json.h>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace eng;
using namespace eng::editor;

namespace {

/// A window holding a toolbar of two buttons, one hidden, and a panel.
struct Window {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiWidgetId toolbar{tree.createWidget(GuiWidgetType::PANEL, root)};
  GuiWidgetId shown{tree.createWidget(GuiWidgetType::BUTTON, toolbar)};
  GuiWidgetId hidden{tree.createWidget(GuiWidgetType::BUTTON, toolbar)};

  Window() {
    tree.findWidget(toolbar)->debug_name = "toolbar";
    tree.findWidget(toolbar)->tree_layout.height = 30.0f;
    tree.findWidget(shown)->id = "select";
    tree.findWidget(shown)->selected = true;
    tree.findWidget(hidden)->visible = false;
    tree.createWidget(GuiWidgetType::PANEL, root);
    tree.computeLayout({0, 0, 400, 300});
  }
  json describe(const EditorWidgetQuery& query) const {
    return json::parse(editorWidgetsJson(tree, query));
  }
};

}  // namespace

TEST_CASE("get_widgets lists the tree with rects and flags, shown only") {
  const Window w;
  const json out = w.describe({});

  CHECK(out["count"] == 4);
  const json& toolbar = out["widgets"]["children"][0];
  CHECK(toolbar["name"] == "toolbar");
  CHECK(toolbar["rect"] == json::array({0.0, 0.0, 400.0, 30.0}));
  REQUIRE(toolbar["children"].size() == 1);
  CHECK(toolbar["children"][0]["id"] == "select");
  CHECK(toolbar["children"][0]["type"] == "button");
  CHECK(toolbar["children"][0]["selected"] == true);
}

TEST_CASE("get_widgets starts under a widget, to a depth, hidden or not") {
  const Window w;

  const json under = w.describe({.under = "toolbar", .hidden = true});
  CHECK(under["widgets"]["name"] == "toolbar");
  CHECK(under["widgets"]["children"].size() == 2);

  const json shallow = w.describe({.depth = 0});
  CHECK(shallow["count"] == 1);
  CHECK(shallow["widgets"]["more"] == 2);

  const json missing = w.describe({.under = "nowhere"});
  CHECK(missing["widgets"].is_null());
  CHECK(missing["error"] == "no widget has the id or name 'nowhere'");
}
