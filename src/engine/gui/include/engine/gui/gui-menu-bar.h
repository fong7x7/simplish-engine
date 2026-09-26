#pragma once

/// @file gui-menu-bar.h
/// @brief A bar of menu titles, each opening its menu below it.
/// @par Threading
/// Main thread only.

#include "gui-menu.h"
#include "gui-panel.h"
#include "gui-widget-tree.h"

#include <vector>

namespace eng {

/// A menu bar: a row of ghost-button titles, each opening its menu — a
/// `GuiDropdown` in the tree's overlay layer, placed under the title and
/// kept on screen — with a see-through scrim behind it that closes it on a
/// click elsewhere — the scrim also watches the pointer, so moving onto
/// another title while a menu is open opens that one instead. A row's
/// `on_select` runs after the menu closes.
///
/// ```cpp
/// auto& bar = *dynamic_cast<GuiMenuBar*>(tree.findWidget(
///     tree.insertExternalWidget(std::make_unique<GuiMenuBar>(), root)));
/// bar.menus = {{"File", {{.label = "Save", .on_select = save}}}};
/// bar.build(tree);        // again after changing `menus`
/// ```
/// @thread_safety Main thread only.
class GuiMenuBar : public GuiPanel {
public:
  /// An empty bar: a padded row, its titles centred up and down.
  GuiMenuBar();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// The theme's surface, with a hairline along the bottom.
  void render(const GuiDrawContext& ctx) const override;

  /// Make a title per menu, and its dropdown and the scrim in the overlay
  /// layer, dropping any made before. Titles point at `menus`' strings:
  /// build again after changing them.
  void build(GuiWidgetTree& tree);

  /// Open menu @p index under its title, closing any other.
  void open(GuiWidgetTree& tree, int index);

  /// Close whatever menu is open.
  void close(GuiWidgetTree& tree);

  /// The open menu, or -1.
  [[nodiscard]] int openIndex() const;

  /// Its menus, left to right.
  std::vector<GuiMenu> menus{};

private:
  /// Drop the titles, dropdowns and scrim `build` made.
  void unbuild(GuiWidgetTree& tree);
  /// Make menu @p index's title and dropdown.
  void buildMenu(GuiWidgetTree& tree, std::size_t index);
  /// Make the scrim behind open menus.
  void buildScrim(GuiWidgetTree& tree);
  /// Open the menu whose title is at (@p x, @p y), if it is not open.
  void followPointer(GuiWidgetTree& tree, float x, float y);

  /// Each menu's title button.
  std::vector<GuiWidgetId> titles_{};
  /// Each menu's dropdown.
  std::vector<GuiWidgetId> dropdowns_{};
  /// The scrim behind an open menu.
  GuiWidgetId scrim_ = GUI_WIDGET_ID_INVALID;
  /// The open menu, or -1.
  int open_ = -1;
};

}  // namespace eng
