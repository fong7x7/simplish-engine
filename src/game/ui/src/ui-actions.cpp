#include <algorithm>
#include <game/ui/ui-actions.h>

namespace eng::game {

namespace {

  /// Every action @p node and its children name, into @p out.
  // NOLINTNEXTLINE(misc-no-recursion) -- a screen is a tree, bounded in depth
  void collect(const UiNode& node, std::vector<std::string>& out) {
    if (node.kind == UiNodeKind::BUTTON && !node.action.empty()) {
      out.push_back(node.action);
    }
    for (const UiNode& child : node.children) {
      collect(child, out);
    }
  }

}  // namespace

std::vector<std::string> uiActions(std::span<const UiScreen> screens) {
  std::vector<std::string> actions;
  for (const UiScreen& screen : screens) {
    collect(screen.root, actions);
  }
  std::ranges::sort(actions);
  const auto [first, last] = std::ranges::unique(actions);
  actions.erase(first, last);
  return actions;
}

}  // namespace eng::game
