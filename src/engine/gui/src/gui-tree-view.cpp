#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-tree-view.h>

namespace eng {

namespace {

  /// Width of the arrow's column, and the padding before it.
  constexpr float ARROW = 14.0f;
  constexpr float PAD = 6.0f;
  /// ▸ and ▾.
  constexpr std::string_view CLOSED = "\xE2\x96\xB8";
  constexpr std::string_view OPEN = "\xE2\x96\xBE";

  /// Append @p nodes' shown paths, under @p prefix, to @p out.
  // NOLINTNEXTLINE(misc-no-recursion) -- a tree is walked by recursion
  void flatten(const std::vector<GuiTreeNode>& nodes,
               std::vector<std::size_t>& prefix,
               std::vector<std::vector<std::size_t>>& out) {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      prefix.push_back(i);
      out.push_back(prefix);
      if (nodes[i].expanded) {
        flatten(nodes[i].children, prefix, out);
      }
      prefix.pop_back();
    }
  }

  /// Where a row's arrow starts, at depth @p depth.
  float arrowX(const Rect& row, std::size_t depth, float indent) {
    return row.x + PAD + static_cast<float>(depth) * indent;
  }

  /// Where a node's row is drawn.
  struct NodeSpot {
    /// The row.
    Rect row;
    /// Where its arrow starts, after the indent.
    float x = 0.0f;
    /// Its opacity.
    float opacity = 1.0f;
  };

  /// Draw @p node's arrow, if it has children, and label at @p spot.
  void drawNode(const GuiDrawContext& ctx, const GuiTreeNode& node,
                const NodeSpot& spot) {
    const GuiTheme& t = ctx.activeTheme();
    const GuiFont& font = t.font(GuiTextRole::BODY);
    const float y =
        spot.row.y + (spot.row.h - ctx.fontMetrics(font).line_height) * 0.5f;
    if (!node.children.empty()) {
      ctx.drawText(
          {.text = node.expanded ? OPEN : CLOSED,
           .pos = {spot.x, y},
           .color = GuiColor::applyOpacity(t.palette.text_muted, spot.opacity),
           .font = font});
    }
    ctx.drawText({.text = node.label,
                  .pos = {spot.x + ARROW, y},
                  .color = GuiColor::applyOpacity(t.palette.text, spot.opacity),
                  .font = font});
  }

}  // namespace

std::unique_ptr<GuiWidget> GuiTreeView::clone() const {
  return std::make_unique<GuiTreeView>(*this);
}

void GuiTreeView::refresh() {
  paths_.clear();
  std::vector<std::size_t> prefix;
  flatten(roots, prefix, paths_);
  row_count = paths_.size();
  current_ = std::min(current_, row_count == 0 ? 0 : row_count - 1);
  clampScroll();
}

const std::vector<std::size_t>& GuiTreeView::pathOf(std::size_t row) const {
  return paths_.at(row);
}

GuiTreeNode& GuiTreeView::nodeAt(std::size_t row) {
  const std::vector<std::size_t>& path = paths_.at(row);
  GuiTreeNode* node = &roots.at(path.front());
  for (std::size_t i = 1; i < path.size(); ++i) {
    node = &node->children.at(path[i]);
  }
  return *node;
}

void GuiTreeView::toggle(std::size_t row) {
  GuiTreeNode& node = nodeAt(row);
  if (!node.children.empty()) {
    node.expanded = !node.expanded;
    refresh();
  }
}

void GuiTreeView::drawRow(const GuiDrawContext& ctx,
                          const GuiListRow& row) const {
  GuiVirtualList::drawRow(ctx, row);
  const std::vector<std::size_t>& path = paths_.at(row.index);
  drawNode(ctx, nodeOnPath(path),
           {row.rect, arrowX(row.rect, path.size() - 1, indent), opacity});
}

const GuiTreeNode&
GuiTreeView::nodeOnPath(const std::vector<std::size_t>& path) const {
  const GuiTreeNode* node = &roots.at(path.front());
  for (std::size_t i = 1; i < path.size(); ++i) {
    node = &node->children.at(path[i]);
  }
  return *node;
}

bool GuiTreeView::handleClick(const GuiMouseEvent& event) {
  const long row = rowAt(event.x, event.y);
  if (row >= 0) {
    const auto r = static_cast<std::size_t>(row);
    const float x = arrowX(bodyRect(), pathOf(r).size() - 1, indent);
    if (event.x >= x && event.x < x + ARROW) {
      toggle(r);
      return true;
    }
  }
  return GuiVirtualList::handleClick(event);
}

long GuiTreeView::parentRow(std::size_t row) const {
  const std::vector<std::size_t>& path = paths_.at(row);
  for (std::size_t r = row; r-- > 0;) {
    if (paths_[r].size() + 1 == path.size()) {
      return static_cast<long>(r);
    }
  }
  return -1;
}

bool GuiTreeView::handleNav(GuiNavCommand command) {
  if (row_count == 0 || disabled) {
    return false;
  }
  if (command == GuiNavCommand::RIGHT) {
    stepIn();
    return true;
  }
  if (command == GuiNavCommand::LEFT) {
    stepOut();
    return true;
  }
  return GuiVirtualList::handleNav(command);
}

void GuiTreeView::stepIn() {
  GuiTreeNode& node = nodeAt(current_);
  if (node.children.empty()) {
    return;
  }
  if (node.expanded) {
    setCurrent(current_ + 1);
  } else {
    toggle(current_);
  }
}

void GuiTreeView::stepOut() {
  const long parent = parentRow(current_);
  if (nodeAt(current_).expanded) {
    toggle(current_);
  } else if (parent >= 0) {
    setCurrent(static_cast<std::size_t>(parent));
  }
}

}  // namespace eng
