#pragma once

// Design Summary -- EditorMenuBarWidget
//
// Behaviours:
//   - Strip of menu titles (File, Edit, Level, View, Help) below the title
//     bar
//   - Clicking a title opens its dropdown; clicking it again closes it
//   - With a menu open, moving onto another title switches to that menu
//   - Choosing a row raises on_command and closes the menu; rows whose
//     command is not implemented yet are drawn disabled and do nothing
//   - Undo and Redo are enabled only while the action history has something
//     for them to do, which the editor pushes in with setHistory()
//   - The File menu lists recent projects, which raise on_open_recent
//   - The Level menu lists the open project's levels, with a mark on the
//     one being edited; choosing another raises on_open_level
//   - The View menu's two projection rows carry a mark on whichever one the
//     open project is using, and are disabled until one is open
//
// Edge Cases:
//   - init() without a valid parent: nothing is created; every other entry
//     point is a no-op
//   - A click anywhere outside the open menu closes it, via a full-window
//     scrim that is only visible while a menu is open
//   - Empty recent-projects list: the recent rows and their separator are
//     omitted rather than shown empty
//   - No project open: there are no level rows, so the Level menu is its
//     one disabled New Level row
//   - A project holding more levels than the menu shows lists the first
//     LEVEL_MENU_MAX of them; a level browser panel is what removes the cap
//
// Invariants:
//   - Dropdowns and the scrim are siblings of this widget, not children.
//     Hit testing never descends into a child that falls outside its
//     parent's rect, and a menu hangs below the bar by definition
//   - At most one dropdown is visible at a time
//   - Every widget this bar creates is destroyed in shutdown()
//
// Integration Points:
//   - SimplishEditor: owns this widget, drives layout()/tick(), and executes
//     the commands it raises

#include <cstddef>
#include <cstdint>
#include <editor/project/project-projection.h>
#include <editor/project/project-shading.h>
#include <editor/project/recent-projects-list.h>
#include <editor/shell/editor-action-history.h>
#include <editor/shell/editor-level-entry.h>
#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-play-mode.h>
#include <engine/gui/gui-dropdown.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-widget-id.h>
#include <engine/gui/gui-widget-tree.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// Height of the menu bar strip in logical pixels.
inline constexpr float MENU_BAR_HEIGHT = 26.0f;

/// Whether a project is open, which gates the rows that need one.
/// @thread_safety Immutable value type.
enum class EditorProjectPresence : uint8_t {
  /// No project is open.
  NONE,
  /// A project is open.
  OPEN,
};

/// Menu bar with one dropdown per title.
/// @thread_safety Main-thread only.
class EditorMenuBarWidget : public GuiPanel {
public:
  EditorMenuBarWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Create the title buttons, the dropdowns, and the click-away scrim.
  void init(GuiWidgetTree& tree);

  /// Position the bar in @p bar_rect and the scrim over @p window.
  void layout(GuiWidgetTree& tree, const Rect& bar_rect, const Rect& window);

  /// Route the tree's arrange pass to layout() so the default column
  /// arrangement does not slice the row into vertical strips.
  void arrangeChildren(GuiWidgetTree& tree, const Rect& available) override;

  /// Rebuild dirty menus and follow the cursor across titles.
  void tick(GuiWidgetTree& tree);

  /// Remove every widget this bar created from @p tree.
  void shutdown(GuiWidgetTree& tree);

  /// Open the menu at @p index, closing any other. Out-of-range closes all.
  void openMenu(GuiWidgetTree& tree, int index);

  /// Close whichever menu is open.
  void closeMenu(GuiWidgetTree& tree);

  /// Index of the open menu, or -1 when none is.
  [[nodiscard]] int openMenuIndex() const { return open_menu_; }

  /// Number of menu titles.
  [[nodiscard]] size_t menuCount() const { return menus_.size(); }

  /// Dropdown widget id for the menu at @p index, for tests and hit routing.
  [[nodiscard]] GuiWidgetId dropdownId(size_t index) const;

  /// Title button id for the menu at @p index.
  [[nodiscard]] GuiWidgetId titleButtonId(size_t index) const;

  /// Replace the recent-projects rows in the File menu.
  void setRecentProjects(const RecentProjectsList& recent);

  /// Replace the Level menu's rows with @p levels, marking @p current.
  ///
  /// A no-op when neither has changed: this is called after every edit, on
  /// the same path that pushes the undo history in, and rebuilding the
  /// menus each time would throw away a hovered row for nothing.
  void setLevels(const std::vector<EditorLevelEntry>& levels,
                 std::string_view current);

  /// Enable or disable the rows that need an open project.
  void setProjectPresence(EditorProjectPresence presence);

  /// Mark whichever projection row @p projection names as the live one.
  void setProjection(ProjectProjection projection);

  /// Mark whichever shading row @p shading names as the live one.
  void setShading(ProjectShading shading);

  /// Gate the Undo and Redo rows on what @p history holds.
  ///
  /// Takes the history rather than two flags so the bar cannot be told a
  /// state the history is not actually in, and copies nothing out of it:
  /// the history is unbounded and this is called after every edit.
  void setHistory(const EditorActionHistory& history);

  /// Tick Play Level while the level is being played, so the menu says
  /// what the toolbar's Stop button says.
  void setPlayMode(EditorPlayMode mode);

  /// Raised when a row is chosen. Never called with SEPARATOR.
  std::function<void(EditorMenuCommand)> on_command{};

  /// Raised when a recent-project row is chosen, with that project's path.
  std::function<void(std::string_view)> on_open_recent{};

  /// Raised when a level row is chosen, with that level's id. Never called
  /// for the level already being edited.
  std::function<void(std::string_view)> on_open_level{};

private:
  /// One title and the dropdown it opens.
  struct Menu {
    /// Text on the title button.
    std::string_view title{};
    /// Title button in the bar.
    GuiWidgetId button = GUI_WIDGET_ID_INVALID;
    /// Dropdown panel, a sibling of the bar.
    GuiWidgetId dropdown = GUI_WIDGET_ID_INVALID;
  };

  /// Create one title button and its dropdown.
  void wireMenu(GuiWidgetTree& tree, size_t index);
  /// Create and style the title button for the menu at @p index.
  void wireTitleButton(GuiWidgetTree& tree, size_t index);
  /// Style the already-created dropdown for the menu at @p index.
  void wireDropdown(GuiWidgetTree& tree, size_t index);
  /// Attach the hover and select handlers to @p view.
  void wireDropdownHandlers(GuiDropdown& view);
  /// Create the full-window click-away scrim.
  void wireScrim(GuiWidgetTree& tree);
  /// Rebuild the rows of every menu.
  void rebuildItems(GuiWidgetTree& tree);
  /// Fill the dropdown for the menu at @p index with its rows.
  void buildItems(GuiWidgetTree& tree, size_t index);
  /// Append the menu at @p index's rows, dynamic blocks included, to an
  /// already-emptied @p menu.
  void buildRows(GuiDropdown& menu, size_t index);
  /// Append one command row to @p menu, disabled when unimplemented.
  void appendCommand(GuiDropdown& menu, EditorMenuCommand command);
  /// Append the recent-project rows to the File menu's dropdown.
  void appendRecentItems(GuiDropdown& menu);
  /// Append one recent-project row.
  void appendRecentItem(GuiDropdown& menu, const RecentProjectEntry& entry);
  /// Append the level rows to the Level menu's dropdown.
  void appendLevelItems(GuiDropdown& menu);
  /// Append one level row, marked when it is the one being edited.
  void appendLevelItem(GuiDropdown& menu, const EditorLevelEntry& level);
  /// Size and place the dropdown at @p index under its title button.
  void placeDropdown(GuiWidgetTree& tree, size_t index);
  /// Ask for @p index to be open after the next tick. Click handlers cannot
  /// touch the tree — they never receive one — so they record the intent
  /// here and tick() applies it.
  void requestMenu(int index);
  /// Follow the cursor across titles and drop a stale row highlight.
  void syncHover(GuiWidgetTree& tree);
  /// Whether @p command can do anything in the current editor state.
  [[nodiscard]] bool commandEnabled(EditorMenuCommand command) const;
  /// Whether @p command names the setting the editor is currently in.
  [[nodiscard]] bool commandChecked(EditorMenuCommand command) const;

  /// Menus in left-to-right order.
  std::vector<Menu> menus_{};
  /// Full-window panel that closes the menu when clicked.
  GuiWidgetId scrim_ = GUI_WIDGET_ID_INVALID;
  /// Index of the open menu, or -1.
  int open_menu_ = -1;
  /// Menu a click handler asked for; applied by the next tick().
  int requested_menu_ = -1;
  /// Whether `requested_menu_` is waiting to be applied.
  bool request_pending_ = false;
  /// Recent projects mirrored into the File menu.
  RecentProjectsList recent_{};
  /// The open project's levels, mirrored into the Level menu.
  std::vector<EditorLevelEntry> levels_{};
  /// Id of the level being edited, which is the marked row.
  std::string current_level_{};
  /// Whether a project is open, which gates some rows.
  EditorProjectPresence project_ = EditorProjectPresence::NONE;
  /// The open project's projection, mirrored so its row shows a mark.
  ProjectProjection projection_ = ProjectProjection::DIMETRIC;
  /// The open project's shading, mirrored so its row shows a mark.
  ProjectShading shading_ = ProjectShading::SMOOTH;
  /// Whether the history has an applied action for Undo to revert.
  bool can_undo_ = false;
  /// Whether the history has a reverted action for Redo to reapply.
  bool can_redo_ = false;
  /// Whether the level is being played, which checks Play Level.
  EditorPlayMode play_mode_ = EditorPlayMode::EDITING;
  /// Set when the rows are stale and tick() must rebuild them.
  bool items_dirty_ = true;
};

}  // namespace eng::editor
