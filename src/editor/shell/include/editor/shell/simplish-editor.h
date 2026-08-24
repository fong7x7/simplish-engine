#pragma once

// Design Summary -- SimplishEditor
//
// Behaviours:
//   - Inherits DesktopGameClient for the platform window, engine init, GUI
//     routing, and per-frame RHI presentation
//   - Builds the editor chrome on init: title bar, toolbar, viewport
//   - Opens a project from a path, updates the recent list, and reflects the
//     project name in the window title and toolbar
//   - Per frame: lays the chrome out for the current window size and pushes
//     hovered-tile and zoom into the toolbar status text
//
// Edge Cases:
//   - No project on the command line: the editor opens with no project and
//     the toolbar shows "No project". Nothing else is disabled — this scope
//     has no project-gated commands yet
//   - openProject failure: the reason is logged and the previous project (if
//     any) stays open
//
// Invariants:
//   - init() runs exactly once before run()
//   - Widgets created in onInit() are destroyed in onShutdown()
//
// Integration Points:
//   - src/bin/editor/src/main.cpp: constructs, initialises, and runs this

#include <cstdint>
#include <editor/shell/editor-shell-state.h>
#include <editor/shell/editor-toolbar-widget.h>
#include <editor/shell/editor-viewport-widget.h>
#include <engine/client/desktop-game-client.h>
#include <engine/gui/gui-widget-id.h>
#include <filesystem>
#include <string>

namespace eng::editor {

/// The editor application.
/// @thread_safety Main-thread only.
class SimplishEditor final : public eng::client::DesktopGameClient {
public:
  /// Open the project at @p root before or after init(). Returns false and
  /// leaves any current project untouched when the open fails.
  bool openProjectAt(const std::filesystem::path& root);

  /// Path the recent-projects list is read from and written to. Must be set
  /// before init() to take effect at startup.
  void setRecentProjectsPath(std::filesystem::path path);

  /// Read-only view of shell state, for tests and the entry point.
  [[nodiscard]] const EditorShellState& state() const { return state_; }

protected:
  bool onInit() override;
  bool onTick(float dt) override;
  void onShutdown() override;
  void onClientKeyDown(uint32_t key, ClientKeyDownKind kind) override;

private:
  /// Create the title bar, toolbar, and viewport under the GUI root.
  void initChrome();
  /// Position the chrome for the current window size.
  void layoutChrome();
  /// Push project name and viewport status into the toolbar.
  void refreshToolbar();
  /// Apply the open project to the window title and toolbar.
  void applyProjectToChrome();

  /// Persistent shell state.
  EditorShellState state_{};
  /// Title bar strip above the toolbar.
  GuiWidgetId title_panel_ = GUI_WIDGET_ID_INVALID;
  /// Title text.
  GuiWidgetId title_label_ = GUI_WIDGET_ID_INVALID;
  /// Toolbar widget id in the tree (owned by the tree).
  GuiWidgetId toolbar_id_ = GUI_WIDGET_ID_INVALID;
  /// Viewport widget id in the tree (owned by the tree).
  GuiWidgetId viewport_id_ = GUI_WIDGET_ID_INVALID;
  /// Backing store for the title label's string_view.
  std::string title_text_{"Simplish Editor"};
  /// Last window size the chrome was laid out for.
  uint32_t laid_out_width_ = 0;
  /// Last window height the chrome was laid out for.
  uint32_t laid_out_height_ = 0;
};

}  // namespace eng::editor
