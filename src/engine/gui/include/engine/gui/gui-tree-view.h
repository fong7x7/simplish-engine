#pragma once

/// @file gui-tree-view.h
/// @brief A virtual list of nested rows that fold and unfold.
/// @par Threading
/// Main thread only.

#include "gui-tree-node.h"
#include "gui-virtual-list.h"

#include <cstddef>
#include <vector>

namespace eng {

/// A tree view: `roots` and their expanded descendants as indented rows of
/// a `GuiVirtualList`, each with a disclosure arrow if it has children.
/// Clicking the arrow folds or unfolds; RIGHT unfolds the current row or
/// steps into it, LEFT folds it or steps out to its parent. Rows are
/// addressed by index path from the roots: `pathOf(row)`. After changing
/// `roots` directly, call `refresh`.
///
/// ```cpp
/// tree->roots = {{"Levels", {{"Station"}, {"Docks"}}, true}};
/// tree->refresh();
/// tree->on_activate = [&](size_t row) { open(tree->pathOf(row)); };
/// ```
/// @thread_safety Main thread only.
class GuiTreeView : public GuiVirtualList {
public:
  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// A click on an arrow folds or unfolds; elsewhere it selects.
  bool handleClick(const GuiMouseEvent& event) override;

  /// RIGHT and LEFT fold, unfold and step; the rest as the list.
  bool handleNav(GuiNavCommand command) override;

  /// Re-read `roots` into rows.
  void refresh();

  /// Fold or unfold the node on row @p row.
  void toggle(std::size_t row);

  /// Row @p row's index path from the roots.
  [[nodiscard]] const std::vector<std::size_t>& pathOf(std::size_t row) const;

  /// The node on row @p row.
  [[nodiscard]] GuiTreeNode& nodeAt(std::size_t row);

  /// The top-level nodes.
  std::vector<GuiTreeNode> roots{};
  /// Pixels each level is indented.
  float indent = 16.0f;

protected:
  /// A row's arrow and label.
  void drawRow(const GuiDrawContext& ctx, const GuiListRow& row) const override;

private:
  /// The row of row @p row's parent, or -1 for a root.
  [[nodiscard]] long parentRow(std::size_t row) const;
  /// The node at @p path.
  [[nodiscard]] const GuiTreeNode&
  nodeOnPath(const std::vector<std::size_t>& path) const;
  /// RIGHT: unfold the current row, or step into its first child.
  void stepIn();
  /// LEFT: fold the current row, or step out to its parent.
  void stepOut();

  /// Every shown node's path, in row order.
  std::vector<std::vector<std::size_t>> paths_{};
};

}  // namespace eng
