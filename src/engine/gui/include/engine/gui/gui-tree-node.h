#pragma once

/// @file gui-tree-node.h
/// @brief One node of a tree view's data.
/// @par Threading
/// Plain data.

#include <string>
#include <vector>

namespace eng {

/// A node of a `GuiTreeView`: a label, its children, and whether they
/// show.
// NOLINTNEXTLINE(misc-no-recursion) -- a node holds nodes; copying recurses
// NOLINTNEXTLINE(misc-no-recursion) -- a node holds nodes; copying recurses
struct GuiTreeNode {
  /// What the row says.
  std::string label{};
  /// Its children, in order.
  std::vector<GuiTreeNode> children{};
  /// Whether its children are shown.
  bool expanded = false;
};

}  // namespace eng
