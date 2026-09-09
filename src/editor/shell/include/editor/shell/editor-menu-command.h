#pragma once

/// @file editor-menu-command.h
/// @brief Commands the editor menu bar can raise, and their display text.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// Every command reachable from the menu bar.
///
/// Some of these name work that does not exist yet (there is one level per
/// project, so nothing to save *as*; no clipboard, so nothing to paste).
/// They are listed anyway and rendered disabled, so the menu shows the
/// shape of the editor rather than only the parts that happen to be built.
/// @thread_safety Immutable value type.
enum class EditorMenuCommand : uint8_t {
  /// Layout marker: draws a divider row and is never dispatched.
  SEPARATOR,
  /// Create a project in a new directory.
  NEW_PROJECT,
  /// Open an existing project directory.
  OPEN_PROJECT,
  /// Write what has been placed to the open project's level file.
  SAVE,
  /// Write the open document to a new path.
  SAVE_AS,
  /// Close the open project, leaving the editor with none.
  CLOSE_PROJECT,
  /// Quit the editor.
  EXIT,
  /// Revert the last edit.
  UNDO,
  /// Reapply the last reverted edit.
  REDO,
  /// Remove the selection to the clipboard.
  CUT,
  /// Copy the selection to the clipboard.
  COPY,
  /// Paste the clipboard at the cursor.
  PASTE,
  /// Remove the selected placement or light from the level.
  ///
  /// Not `DELETE`: `<windows.h>` defines that as an access mask, and a
  /// macro cannot be scoped away by an enum class.
  DELETE_SELECTION,
  /// Open editor preferences.
  SETTINGS,
  /// Return the viewport camera to the origin at 1:1 zoom.
  RESET_VIEW,
  /// Zoom the viewport camera in one step.
  ZOOM_IN,
  /// Zoom the viewport camera out one step.
  ZOOM_OUT,
  /// Show or hide the viewport tile grid.
  TOGGLE_GRID,
  /// Draw the world with the zero-yaw 4:3 dimetric projection.
  SET_VIEW_DIMETRIC,
  /// Draw the world with the 2:1 isometric projection.
  SET_VIEW_ISOMETRIC,
  /// Show build and version information.
  ABOUT,
};

/// Display text for one command.
/// @thread_safety Immutable value type.
struct EditorMenuCommandInfo {
  /// The command this row names.
  EditorMenuCommand command = EditorMenuCommand::SEPARATOR;
  /// Menu row label.
  std::string_view label{};
  /// Accelerator hint, or empty when the command has no key bound.
  ///
  /// Only commands `SimplishEditor::onClientKeyDown` actually handles carry
  /// text here. A hint for a key that does nothing is worse than no hint.
  std::string_view shortcut{};
};

/// Display text for every command, in no particular menu order.
inline constexpr EditorMenuCommandInfo EDITOR_MENU_COMMAND_INFO[] = {
    {EditorMenuCommand::SEPARATOR, "", ""},
    {EditorMenuCommand::NEW_PROJECT, "New Project...", ""},
    {EditorMenuCommand::OPEN_PROJECT, "Open Project...", ""},
#ifdef __APPLE__
    {EditorMenuCommand::SAVE, "Save", "Cmd+S"},
#else
    {EditorMenuCommand::SAVE, "Save", "Ctrl+S"},
#endif
    {EditorMenuCommand::SAVE_AS, "Save As...", ""},
    {EditorMenuCommand::CLOSE_PROJECT, "Close Project", ""},
    {EditorMenuCommand::EXIT, "Exit", ""},
// Both Control and Command work on every platform — the handler accepts
// either. The hint names the one this platform's users expect, which is the
// only thing the choice below decides.
#ifdef __APPLE__
    {EditorMenuCommand::UNDO, "Undo", "Cmd+Z"},
    {EditorMenuCommand::REDO, "Redo", "Cmd+Shift+Z"},
#else
    {EditorMenuCommand::UNDO, "Undo", "Ctrl+Z"},
    {EditorMenuCommand::REDO, "Redo", "Ctrl+Shift+Z"},
#endif
    {EditorMenuCommand::CUT, "Cut", ""},
    {EditorMenuCommand::COPY, "Copy", ""},
    {EditorMenuCommand::PASTE, "Paste", ""},
    {EditorMenuCommand::DELETE_SELECTION, "Delete", "Del"},
    {EditorMenuCommand::SETTINGS, "Settings...", ""},
    {EditorMenuCommand::RESET_VIEW, "Reset View", "0"},
    {EditorMenuCommand::ZOOM_IN, "Zoom In", "="},
    {EditorMenuCommand::ZOOM_OUT, "Zoom Out", "-"},
    {EditorMenuCommand::TOGGLE_GRID, "Toggle Grid", "G"},
    {EditorMenuCommand::SET_VIEW_DIMETRIC, "Dimetric View", ""},
    {EditorMenuCommand::SET_VIEW_ISOMETRIC, "Isometric View", ""},
    {EditorMenuCommand::ABOUT, "About Simplish", ""},
};

/// Look up the display text for @p command.
[[nodiscard]] constexpr const EditorMenuCommandInfo&
editorMenuCommandInfo(EditorMenuCommand command) {
  for (const auto& info : EDITOR_MENU_COMMAND_INFO) {
    if (info.command == command) {
      return info;
    }
  }
  return EDITOR_MENU_COMMAND_INFO[0];
}

/// Menu row label for @p command.
[[nodiscard]] constexpr std::string_view
editorMenuCommandLabel(EditorMenuCommand command) {
  return editorMenuCommandInfo(command).label;
}

/// Accelerator hint for @p command, empty when no key is bound.
[[nodiscard]] constexpr std::string_view
editorMenuCommandShortcut(EditorMenuCommand command) {
  return editorMenuCommandInfo(command).shortcut;
}

}  // namespace eng::editor
