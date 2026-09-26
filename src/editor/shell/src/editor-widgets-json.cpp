#include <editor/shell/editor-widgets-json.h>
#include <iterator>
#include <nlohmann/json.hpp>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The word each widget type is listed as, in `GuiWidgetType` order.
  constexpr std::string_view TYPES[] = {"panel",      "text",      "button",
                                        "text_input", "text_area", "scroll",
                                        "image",      "custom"};
  static_assert(std::size(TYPES) ==
                static_cast<size_t>(GuiWidgetType::CUSTOM) + 1);

  /// The most widgets one answer lists.
  constexpr size_t MAX_LISTED = 2000;

  /// A walk down the tree: what was asked, and how many are listed.
  struct Walk {
    /// The tree walked.
    const GuiWidgetTree& tree;
    /// What was asked.
    const EditorWidgetQuery& query;
    /// How many are listed so far.
    size_t count = 0;
  };

  /// @p widget's own fields: what it is, where, and its flags.
  json ownJson(const GuiWidget& widget) {
    const Rect& r = widget.rect;
    return {{"type", TYPES[static_cast<size_t>(widget.widget_type)]},
            {"id", widget.id},
            {"name", widget.debug_name},
            {"rect", json::array({r.x, r.y, r.w, r.h})},
            {"visible", widget.visible},
            {"disabled", widget.disabled},
            {"selected", widget.selected},
            {"focused", widget.focused},
            {"hovered", widget.hovered}};
  }

  /// Whether @p widget is listed at all.
  bool listed(const Walk& walk, const GuiWidget& widget) {
    return (widget.visible || walk.query.hidden) && walk.count < MAX_LISTED;
  }

  // NOLINTNEXTLINE(misc-no-recursion) -- a widget tree, bounded in depth
  json widgetJson(Walk& walk, const GuiWidget& widget, size_t depth) {
    ++walk.count;
    json out = ownJson(widget);
    if (depth >= walk.query.depth) {
      out["more"] = widget.children.size();
      return out;
    }
    json children = json::array();
    for (const GuiWidgetId id : widget.children) {
      const GuiWidget* child = walk.tree.findWidget(id);
      if (child != nullptr && listed(walk, *child)) {
        children.push_back(widgetJson(walk, *child, depth + 1));
      }
    }
    out["children"] = std::move(children);
    return out;
  }

  /// The widget @p name names by id or debug name — the lowest-numbered,
  /// when several do — or the root for an empty name.
  const GuiWidget* startOf(const GuiWidgetTree& tree, std::string_view name) {
    if (name.empty()) {
      return tree.findWidget(tree.root_id);
    }
    const GuiWidget* found = nullptr;
    for (const auto& entry : tree.widget_nodes) {
      const GuiWidget& w = *entry.second;
      if ((w.id == name || w.debug_name == name) &&
          (found == nullptr || w.widget_id < found->widget_id)) {
        found = &w;
      }
    }
    return found;
  }

}  // namespace

std::string editorWidgetsJson(const GuiWidgetTree& tree,
                              const EditorWidgetQuery& query) {
  const GuiWidget* start = startOf(tree, query.under);
  if (start == nullptr) {
    return json{{"widgets", nullptr},
                {"count", 0},
                {"error", "no widget has the id or name '" + query.under + "'"}}
        .dump(2);
  }
  Walk walk{tree, query};
  const json widgets = widgetJson(walk, *start, 0);
  return json{{"widgets", widgets}, {"count", walk.count}, {"error", ""}}.dump(
      2);
}

}  // namespace eng::editor
