#pragma once

// Design Summary -- SimplishEditor
//
// Behaviours:
//   - Inherits DesktopGameClient for the platform window, engine init, GUI
//     routing, and per-frame RHI presentation
//   - Builds the editor chrome on init: title bar, menu bar, toolbar,
//     viewport, asset panel
//   - Lists the open project's assets, and places one in the world when it
//     is dragged from the panel onto the viewport
//   - Draws placed meshes in a depth-tested scene pass under the interface
//   - Opens a project from a path, updates the recent list, and reflects the
//     project name in the window title and toolbar
//   - File > New Project asks the OS for a location and name, then creates
//     and opens a project there; File > Open Project asks for a folder and
//     opens the project in it
//   - Per frame: lays the chrome out for the current window size and pushes
//     hovered-tile and zoom into the toolbar status text
//
// Edge Cases:
//   - No project on the command line: the editor opens with no project and
//     the toolbar shows "No project". File > Close Project is the one
//     project-gated command, and it is disabled until one is open
//   - Menu commands whose subsystem does not exist yet (save, undo, redo,
//     settings) are listed but disabled; see editor-menu-command.h
//   - openProject failure: the reason is logged and the previous project (if
//     any) stays open
//
// Invariants:
//   - init() runs exactly once before run()
//   - Every chrome widget hangs from a window-sized root panel. Hit testing
//     never descends into a rect that does not contain the cursor, so a
//     smaller root would make everything outside it unclickable
//   - Widgets created in onInit() are destroyed in onShutdown()
//
// Integration Points:
//   - src/bin/editor/src/main.cpp: constructs, initialises, and runs this

#include <cstdint>
#include <editor/project/project-open-error.h>
#include <editor/shell/editor-asset-panel-widget.h>
#include <editor/shell/editor-menu-bar-widget.h>
#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-shell-state.h>
#include <editor/shell/editor-toolbar-widget.h>
#include <editor/shell/editor-viewport-widget.h>
#include <engine/client/desktop-game-client.h>
#include <engine/gui/gui-widget-id.h>
#include <engine/render-mesh/mesh-renderer.h>
#include <filesystem>
#include <string>
#include <vector>

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
  void onSaveLocationChosen(const std::filesystem::path& path) override;
  void onFolderChosen(const std::filesystem::path& path) override;
  [[nodiscard]] RhiTextureHandle sceneDepthTarget() override;
  void recordScene(RhiCommandList& cmd) override;
  bool onTick(float dt) override;
  void onShutdown() override;
  void onClientKeyDown(uint32_t key, ClientKeyDownKind kind) override;

private:
  /// Create the title bar, menu bar, toolbar, and viewport under the root.
  void initChrome();
  /// Create the window-sized panel every other widget hangs from.
  void initRoot(GuiWidgetTree& tree);
  /// Create the title bar panel and its label.
  void initTitleBar(GuiWidgetTree& tree);
  /// Create the menu bar and wire its commands back to this editor.
  void initMenuBar(GuiWidgetTree& tree);
  /// Create the toolbar and the viewport.
  void initWorkArea(GuiWidgetTree& tree);
  /// Create the asset panel and wire its drops back to this editor.
  void initAssetPanel(GuiWidgetTree& tree);
  /// Rescan the open project's assets and refresh the panel.
  void refreshAssets();
  /// Place the asset at @p index at a layout position, if that position is
  /// over the viewport.
  void dropAsset(size_t index, float x, float y);
  /// Load and upload an asset's mesh if it is not on the GPU yet. False
  /// when it cannot be loaded, which is remembered rather than retried.
  bool ensureAssetMesh(size_t index);
  /// Read, orient, and upload one asset's mesh.
  bool loadAssetMesh(EditorAsset& asset);
  /// The whole drawable surface as a GPU viewport.
  [[nodiscard]] RhiViewport surfaceViewport();
  /// Build the draw parameters for this frame's scene pass.
  [[nodiscard]] MeshRenderer::DrawParams
  sceneDrawParams(const EditorViewportWidget& viewport);
  /// Rebuild `scene_instances_` from the current placements.
  void buildSceneInstances();
  /// Push placement footprints into the viewport for its overlay.
  void refreshPlacementMarkers();
  /// The viewport widget, or nullptr before the chrome exists.
  [[nodiscard]] EditorViewportWidget* viewportWidget();
  /// Position the chrome for the current window size.
  void layoutChrome();
  /// Place the title bar and its label across @p window.
  void layoutTitleBar(GuiWidgetTree& tree, const Rect& window);
  /// Place the viewport and the asset strip below @p top.
  void layoutViewportAndAssets(GuiWidgetTree& tree, const Rect& window,
                               float top);
  /// Place the menu bar, toolbar, and viewport down @p window.
  void layoutWorkArea(GuiWidgetTree& tree, const Rect& window);
  /// Push project name and viewport status into the toolbar.
  void refreshToolbar();
  /// Apply the open project to the window title, menu bar, and toolbar.
  void applyProjectToChrome();
  /// Push the open project's state into the chrome widgets.
  void applyProjectToWidgets();
  /// Carry out one menu command.
  void executeCommand(EditorMenuCommand command);
  /// Carry out the File menu's project commands. Returns false when the
  /// command belongs to another menu.
  bool runProjectCommand(EditorMenuCommand command);
  /// Open the OS dialog a command needs. Returns false for commands that
  /// need no dialog.
  bool runDialogCommand(EditorMenuCommand command);
  /// Stamp the manifest and promote the project in the recent list.
  void recordProjectOpened(const std::string& stamp);
  /// Carry out the View menu's camera and grid commands.
  void applyViewCommand(EditorMenuCommand command);
  /// Close the open project, leaving the editor with none.
  void closeProject();
  /// Create a project at @p root and open it. The directory's own name
  /// becomes the project name, which is what the user just typed into the
  /// dialog.
  bool createProjectAt(const std::filesystem::path& root);
  /// Show build information in the toolbar status line for a few seconds.
  void showAbout();
  /// Put a message in the toolbar status line for a few seconds.
  void showStatusMessage(std::string text);
  /// Log and surface why a project could not be opened.
  void reportProjectOpenFailure(ProjectOpenError error);
  /// Run the View accelerators. Returns true when @p key was one of them.
  bool handleViewKey(uint32_t key);
  /// Let the chrome widgets release what they own, then destroy them.
  void shutdownChrome();
  /// Destroy the chrome nodes and forget their ids.
  void destroyChromeWidgets(GuiWidgetTree& tree);

  /// Persistent shell state.
  EditorShellState state_{};
  /// Window-sized tree root; hit testing is bounded by its rect.
  GuiWidgetId root_panel_ = GUI_WIDGET_ID_INVALID;
  /// Title bar strip above the toolbar.
  GuiWidgetId title_panel_ = GUI_WIDGET_ID_INVALID;
  /// Title text.
  GuiWidgetId title_label_ = GUI_WIDGET_ID_INVALID;
  /// Menu bar widget id in the tree (owned by the tree).
  GuiWidgetId menu_bar_id_ = GUI_WIDGET_ID_INVALID;
  /// Toolbar widget id in the tree (owned by the tree).
  GuiWidgetId toolbar_id_ = GUI_WIDGET_ID_INVALID;
  /// Viewport widget id in the tree (owned by the tree).
  GuiWidgetId viewport_id_ = GUI_WIDGET_ID_INVALID;
  /// Asset panel widget id in the tree (owned by the tree).
  GuiWidgetId asset_panel_id_ = GUI_WIDGET_ID_INVALID;
  /// Mesh pipeline, uploaded meshes, and the scene depth target.
  MeshRenderer mesh_renderer_{};
  /// Instances rebuilt each frame from the placements. Kept as a member so
  /// a frame does not allocate.
  std::vector<MeshInstance> scene_instances_{};
  /// Backing store for the title label's string_view.
  std::string title_text_{"Simplish Editor"};
  /// Message shown in place of the toolbar status while it lasts.
  std::string status_override_{};
  /// Seconds `status_override_` still has to run.
  float status_override_left_ = 0.0f;
  /// Set by File > Exit; onTick returns false once it is true.
  bool quit_requested_ = false;
  /// Last window size the chrome was laid out for.
  uint32_t laid_out_width_ = 0;
  /// Last window height the chrome was laid out for.
  uint32_t laid_out_height_ = 0;
};

}  // namespace eng::editor
