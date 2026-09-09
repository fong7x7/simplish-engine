#include <algorithm>
#include <cstddef>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-menu-availability.h>
#include <editor/shell/editor-menu-bar-widget.h>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-theme-constants.h>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  constexpr float TITLE_WIDTH = 52.0f;
  constexpr float TITLE_HEIGHT = 20.0f;
  constexpr float TITLE_GAP = 2.0f;
  constexpr float SIDE_PADDING = 8.0f;

  constexpr int MENU_WIDTH = 216;
  constexpr int MENU_ITEM_HEIGHT = 24;

  /// Paint and hit-test order for the widgets this bar owns. All three sit
  /// above the viewport, which leaves its z_index at the default 0.
  constexpr int32_t SCRIM_Z = 100;
  constexpr int32_t DROPDOWN_Z = 101;
  constexpr int32_t MENU_BAR_Z = 102;

  /// Most recent projects offered in the File menu.
  constexpr size_t RECENT_MENU_MAX = 5;
  /// Longest recent-project name shown before it is elided.
  constexpr size_t RECENT_LABEL_MAX = 26;

  /// Sentinel for "this menu has no recent-projects block".
  constexpr size_t NO_RECENT_BLOCK = static_cast<size_t>(-1);

  /// One menu's title and the rows under it.
  struct MenuSpec {
    /// Title drawn on the bar.
    std::string_view title;
    /// Rows in order; SEPARATOR draws a divider.
    const EditorMenuCommand* commands;
    /// Number of entries in `commands`.
    size_t count;
    /// Row index after which the recent-projects block is spliced in.
    size_t recent_after;
  };

  constexpr EditorMenuCommand FILE_ROWS[] = {
      EditorMenuCommand::NEW_PROJECT,   EditorMenuCommand::OPEN_PROJECT,
      EditorMenuCommand::SEPARATOR,     EditorMenuCommand::SAVE,
      EditorMenuCommand::SAVE_AS,       EditorMenuCommand::SEPARATOR,
      EditorMenuCommand::CLOSE_PROJECT, EditorMenuCommand::SEPARATOR,
      EditorMenuCommand::EXIT,
  };

  constexpr EditorMenuCommand EDIT_ROWS[] = {
      EditorMenuCommand::UNDO,      EditorMenuCommand::REDO,
      EditorMenuCommand::SEPARATOR, EditorMenuCommand::CUT,
      EditorMenuCommand::COPY,      EditorMenuCommand::PASTE,
      EditorMenuCommand::SEPARATOR, EditorMenuCommand::SETTINGS,
  };

  constexpr EditorMenuCommand VIEW_ROWS[] = {
      EditorMenuCommand::RESET_VIEW,
      EditorMenuCommand::ZOOM_IN,
      EditorMenuCommand::ZOOM_OUT,
      EditorMenuCommand::SEPARATOR,
      EditorMenuCommand::TOGGLE_GRID,
      EditorMenuCommand::SEPARATOR,
      EditorMenuCommand::SET_VIEW_DIMETRIC,
      EditorMenuCommand::SET_VIEW_ISOMETRIC,
  };

  constexpr EditorMenuCommand HELP_ROWS[] = {EditorMenuCommand::ABOUT};

  /// The menu bar, left to right. Recent projects follow "Open Project..."
  /// in the File menu, which is where every editor puts them.
  constexpr MenuSpec MENU_SPECS[] = {
      {"File", FILE_ROWS, std::size(FILE_ROWS), 1},
      {"Edit", EDIT_ROWS, std::size(EDIT_ROWS), NO_RECENT_BLOCK},
      {"View", VIEW_ROWS, std::size(VIEW_ROWS), NO_RECENT_BLOCK},
      {"Help", HELP_ROWS, std::size(HELP_ROWS), NO_RECENT_BLOCK},
  };

  GuiButtonStyle titleStyle() {
    return {THEME_BG, THEME_TEXT, THEME_HOVER, 0.0f};
  }

  GuiDropdownStyle menuStyle() {
    return {THEME_PANEL, THEME_TEXT, THEME_ACCENT, MENU_WIDTH,
            MENU_ITEM_HEIGHT};
  }

  /// Shorten a project name that would overrun the menu width.
  std::string elide(std::string_view name) {
    if (name.size() <= RECENT_LABEL_MAX) {
      return std::string(name);
    }
    return std::string(name.substr(0, RECENT_LABEL_MAX - 3)).append("...");
  }

}  // namespace

EditorMenuBarWidget::EditorMenuBarWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-menu-bar";
  fill_color = THEME_BG;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  z_index = MENU_BAR_Z;
}

std::unique_ptr<GuiWidget> EditorMenuBarWidget::clone() const {
  return std::make_unique<EditorMenuBarWidget>(*this);
}

void EditorMenuBarWidget::init(GuiWidgetTree& tree) {
  if (widget_id == GUI_WIDGET_ID_INVALID) {
    return;
  }
  menus_.clear();
  menus_.reserve(std::size(MENU_SPECS));
  for (const auto& spec : MENU_SPECS) {
    menus_.push_back(
        Menu{spec.title, GUI_WIDGET_ID_INVALID, GUI_WIDGET_ID_INVALID});
  }
  for (size_t i = 0; i < menus_.size(); ++i) {
    wireMenu(tree, i);
  }
  wireScrim(tree);
  rebuildItems(tree);
}

void EditorMenuBarWidget::wireMenu(GuiWidgetTree& tree, size_t index) {
  wireTitleButton(tree, index);
  // A dropdown hangs below the bar, so it cannot be a child of the bar:
  // hit testing never descends into a child outside its parent's rect.
  menus_[index].dropdown =
      tree.insertExternalWidget(std::make_unique<GuiDropdown>(), parent_id);
  wireDropdown(tree, index);
}

void EditorMenuBarWidget::wireTitleButton(GuiWidgetTree& tree, size_t index) {
  Menu& menu = menus_[index];
  menu.button = tree.createWidget(GuiWidgetType::BUTTON, widget_id);
  auto* button = dynamic_cast<GuiButton*>(tree.findWidget(menu.button));
  if (button == nullptr) {
    return;
  }
  button->label = menu.title;
  button->debug_name = std::string(menu.title);
  button->style = titleStyle();
  button->override_style = true;
  const auto slot = static_cast<int>(index);
  button->onClick([this, slot](const GuiMouseEvent&) {
    requestMenu(open_menu_ == slot ? -1 : slot);
  });
}

void EditorMenuBarWidget::wireDropdown(GuiWidgetTree& tree, size_t index) {
  auto* view =
      dynamic_cast<GuiDropdown*>(tree.findWidget(menus_[index].dropdown));
  if (view == nullptr) {
    return;
  }
  view->debug_name = std::string(menus_[index].title).append("-menu");
  view->style = menuStyle();
  view->override_style = true;
  view->border_color = THEME_BORDER;
  view->border_width = 1.0f;
  view->z_index = DROPDOWN_Z;
  view->visible = false;
  wireDropdownHandlers(*view);
}

void EditorMenuBarWidget::wireDropdownHandlers(GuiDropdown& view) {
  // Both handlers live on `view` itself, so capturing it cannot outlive it.
  view.onMouseMove([target = &view](const GuiMouseEvent& event) {
    target->hovered_item = target->hitTestItem(event.x, event.y);
  });
  view.onClick([this, target = &view](const GuiMouseEvent& event) {
    target->selectItem(target->hitTestItem(event.x, event.y));
    requestMenu(-1);
  });
}

void EditorMenuBarWidget::wireScrim(GuiWidgetTree& tree) {
  scrim_ = tree.createWidget(GuiWidgetType::PANEL, parent_id);
  auto* panel = dynamic_cast<GuiPanel*>(tree.findWidget(scrim_));
  if (panel == nullptr) {
    return;
  }
  panel->debug_name = "editor-menu-scrim";
  // Fully transparent: it exists to catch the click that dismisses a menu,
  // not to dim the editor behind it.
  panel->fill_color = GuiColor{0, 0, 0, 0};
  panel->z_index = SCRIM_Z;
  panel->visible = false;
  panel->onClick([this](const GuiMouseEvent&) { requestMenu(-1); });
}

void EditorMenuBarWidget::appendCommand(GuiDropdown& menu,
                                        EditorMenuCommand command) {
  if (command == EditorMenuCommand::SEPARATOR) {
    menu.items.push_back({.separator = true});
    return;
  }
  const auto& info = editorMenuCommandInfo(command);
  menu.items.push_back({.label = std::string(info.label),
                        .on_select =
                            [this, command]() {
                              if (on_command) {
                                on_command(command);
                              }
                            },
                        .enabled = commandEnabled(command),
                        .shortcut = std::string(info.shortcut),
                        .checked = commandChecked(command)});
}

void EditorMenuBarWidget::appendRecentItems(GuiDropdown& menu) {
  if (recent_.entries.empty()) {
    return;
  }
  menu.items.push_back({.separator = true});
  const size_t shown = std::min(recent_.entries.size(), RECENT_MENU_MAX);
  for (size_t i = 0; i < shown; ++i) {
    appendRecentItem(menu, recent_.entries[i]);
  }
}

void EditorMenuBarWidget::appendRecentItem(GuiDropdown& menu,
                                           const RecentProjectEntry& entry) {
  menu.items.push_back(
      {.label = elide(entry.name), .on_select = [this, path = entry.path]() {
         if (on_open_recent) {
           on_open_recent(path);
         }
       }});
}

void EditorMenuBarWidget::buildItems(GuiWidgetTree& tree, size_t index) {
  auto* menu =
      dynamic_cast<GuiDropdown*>(tree.findWidget(menus_[index].dropdown));
  if (menu == nullptr) {
    return;
  }
  const MenuSpec& spec = MENU_SPECS[index];
  menu->items.clear();
  menu->hovered_item = -1;
  for (size_t row = 0; row < spec.count; ++row) {
    appendCommand(*menu, spec.commands[row]);
    if (row == spec.recent_after) {
      appendRecentItems(*menu);
    }
  }
  placeDropdown(tree, index);
}

void EditorMenuBarWidget::rebuildItems(GuiWidgetTree& tree) {
  for (size_t i = 0; i < menus_.size(); ++i) {
    buildItems(tree, i);
  }
  items_dirty_ = false;
}

bool EditorMenuBarWidget::commandEnabled(EditorMenuCommand command) const {
  // The state-dependent rows first: each is built, and each would still do
  // nothing if it were live right now.
  if (command == EditorMenuCommand::CLOSE_PROJECT ||
      command == EditorMenuCommand::SET_VIEW_DIMETRIC ||
      command == EditorMenuCommand::SET_VIEW_ISOMETRIC) {
    return project_ == EditorProjectPresence::OPEN;
  }
  if (command == EditorMenuCommand::UNDO) {
    return can_undo_;
  }
  if (command == EditorMenuCommand::REDO) {
    return can_redo_;
  }
  return editorMenuCommandImplemented(command);
}

bool EditorMenuBarWidget::commandChecked(EditorMenuCommand command) const {
  if (command == EditorMenuCommand::SET_VIEW_DIMETRIC) {
    return projection_ == ProjectProjection::DIMETRIC;
  }
  if (command == EditorMenuCommand::SET_VIEW_ISOMETRIC) {
    return projection_ == ProjectProjection::ISOMETRIC;
  }
  return false;
}

void EditorMenuBarWidget::setProjection(ProjectProjection projection) {
  if (projection_ == projection) {
    return;
  }
  projection_ = projection;
  items_dirty_ = true;
}

void EditorMenuBarWidget::placeDropdown(GuiWidgetTree& tree, size_t index) {
  auto* menu =
      dynamic_cast<GuiDropdown*>(tree.findWidget(menus_[index].dropdown));
  const GuiWidget* button = tree.findWidget(menus_[index].button);
  if (menu == nullptr || button == nullptr) {
    return;
  }
  // The rect is what hit testing uses; GuiDropdown paints from its style
  // width and row height, so the two must agree.
  const auto rows = static_cast<float>(menu->items.size());
  menu->rect = makeRect(button->rect.x, rect.y + rect.h,
                        static_cast<float>(menu->style.width),
                        rows * static_cast<float>(menu->style.item_height));
}

void EditorMenuBarWidget::layout(GuiWidgetTree& tree, const Rect& bar_rect,
                                 const Rect& window) {
  rect = bar_rect;
  float cursor_x = bar_rect.x + SIDE_PADDING;
  const float button_y = bar_rect.y + (bar_rect.h - TITLE_HEIGHT) * 0.5f;
  for (size_t i = 0; i < menus_.size(); ++i) {
    if (auto* button = tree.findWidget(menus_[i].button)) {
      button->rect = makeRect(cursor_x, button_y, TITLE_WIDTH, TITLE_HEIGHT);
    }
    cursor_x += TITLE_WIDTH + TITLE_GAP;
    placeDropdown(tree, i);
  }
  if (auto* panel = tree.findWidget(scrim_)) {
    panel->rect = window;
  }
}

void EditorMenuBarWidget::arrangeChildren(GuiWidgetTree& tree,
                                          const Rect& available) {
  layout(tree, available, available);
}

void EditorMenuBarWidget::requestMenu(int index) {
  requested_menu_ = index;
  request_pending_ = true;
}

void EditorMenuBarWidget::openMenu(GuiWidgetTree& tree, int index) {
  const auto count = static_cast<int>(menus_.size());
  open_menu_ = (index >= 0 && index < count) ? index : -1;
  for (size_t i = 0; i < menus_.size(); ++i) {
    auto* menu =
        dynamic_cast<GuiDropdown*>(tree.findWidget(menus_[i].dropdown));
    if (menu == nullptr) {
      continue;
    }
    menu->visible = (static_cast<int>(i) == open_menu_);
    if (!menu->visible) {
      menu->hovered_item = -1;
    }
  }
  if (auto* panel = tree.findWidget(scrim_)) {
    panel->visible = open_menu_ >= 0;
  }
}

void EditorMenuBarWidget::closeMenu(GuiWidgetTree& tree) {
  openMenu(tree, -1);
}

void EditorMenuBarWidget::syncHover(GuiWidgetTree& tree) {
  if (open_menu_ < 0) {
    return;
  }
  for (size_t i = 0; i < menus_.size(); ++i) {
    const GuiWidget* button = tree.findWidget(menus_[i].button);
    if (button != nullptr && button->hovered &&
        static_cast<int>(i) != open_menu_) {
      openMenu(tree, static_cast<int>(i));
      return;
    }
  }
  auto* menu = dynamic_cast<GuiDropdown*>(
      tree.findWidget(menus_[static_cast<size_t>(open_menu_)].dropdown));
  if (menu != nullptr && !menu->hovered) {
    menu->hovered_item = -1;
  }
}

void EditorMenuBarWidget::tick(GuiWidgetTree& tree) {
  if (menus_.empty()) {
    return;
  }
  if (items_dirty_) {
    rebuildItems(tree);
  }
  if (request_pending_) {
    request_pending_ = false;
    openMenu(tree, requested_menu_);
  }
  syncHover(tree);
}

void EditorMenuBarWidget::shutdown(GuiWidgetTree& tree) {
  for (const Menu& menu : menus_) {
    tree.destroyWidget(menu.button);
    tree.destroyWidget(menu.dropdown);
  }
  menus_.clear();
  tree.destroyWidget(scrim_);
  scrim_ = GUI_WIDGET_ID_INVALID;
  open_menu_ = -1;
  request_pending_ = false;
}

GuiWidgetId EditorMenuBarWidget::dropdownId(size_t index) const {
  return index < menus_.size() ? menus_[index].dropdown : GUI_WIDGET_ID_INVALID;
}

GuiWidgetId EditorMenuBarWidget::titleButtonId(size_t index) const {
  return index < menus_.size() ? menus_[index].button : GUI_WIDGET_ID_INVALID;
}

void EditorMenuBarWidget::setRecentProjects(const RecentProjectsList& recent) {
  recent_ = recent;
  items_dirty_ = true;
}

void EditorMenuBarWidget::setProjectPresence(EditorProjectPresence presence) {
  if (project_ == presence) {
    return;
  }
  project_ = presence;
  items_dirty_ = true;
}

void EditorMenuBarWidget::setHistory(const EditorActionHistory& history) {
  const bool undo = canUndoEditorAction(history);
  const bool redo = canRedoEditorAction(history);
  if (undo == can_undo_ && redo == can_redo_) {
    return;
  }
  can_undo_ = undo;
  can_redo_ = redo;
  items_dirty_ = true;
}

}  // namespace eng::editor
