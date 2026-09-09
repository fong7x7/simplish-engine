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
//   - Lists the built-in general section above them, whose light sources
//     drop into the world the same way and light every mesh in the scene
//   - Clicking a placed asset or a light selects it, outlines it in the
//     viewport, and opens a properties panel down the right; the panel
//     moves and turns a placement, and aims, dims and tints a light, and
//     clicking bare ground or pressing Escape puts the panel away again
//   - Records every placement and every light as an action, which Edit >
//     Undo reverts and Edit > Redo reapplies; both rows are live only when
//     they would do something, and both answer to Ctrl/Cmd+Z and
//     Ctrl/Cmd+Shift+Z
//   - Draws placed meshes in a depth-tested scene pass under the interface,
//     shaded by the lights the level holds
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
//   - Menu commands whose subsystem does not exist yet (save, cut, copy,
//     paste, settings) are listed but disabled; see editor-menu-command.h
//   - A rescan renumbers the asset list, so it drops the document and the
//     history together: an action holding an old index would otherwise undo
//     into the new list, and the selection goes with them
//   - A scene with no lights in it is lit by the renderer's built-in key
//     light, so a project nobody has lit looks as it always did
//   - Undo and redo renumber one of the document's lists, so the selection
//     is moved with it rather than left pointing at whatever took the slot
//   - A drag on a property scrubs a value continuously but records one
//     history entry, taken against the entry as it was when the drag began
//   - A drag ended by something other than the mouse — Escape, a click on
//     another prop, an undo — is recorded rather than dropped: the value it
//     reached is already in the document, and an unrecorded change is one
//     nothing can undo
//   - openProject failure: the reason is logged and the previous project (if
//     any) stays open
//   - Undo and redo are the one pair of keys that act on OS key-repeat, so
//     holding the accelerator walks back through a run of edits. Every
//     other key here is first-press only
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
#include <editor/shell/editor-asset-browser-widget.h>
#include <editor/shell/editor-general-item.h>
#include <editor/shell/editor-menu-bar-widget.h>
#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-properties-widget.h>
#include <editor/shell/editor-property-edit.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-shell-state.h>
#include <editor/shell/editor-toolbar-widget.h>
#include <editor/shell/editor-viewport-widget.h>
#include <engine/client/desktop-game-client.h>
#include <engine/gui/gui-widget-id.h>
#include <engine/gui/image-data.h>
#include <engine/render-mesh/mesh-renderer.h>
#include <filesystem>
#include <optional>
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

  /// Path the recent-projects list is read from and written to, and which
  /// the list is read from as soon as it is known. Must be set before
  /// init() to take effect at startup.
  void setRecentProjectsPath(std::filesystem::path path);

  /// Read-only view of shell state, for tests and the entry point.
  [[nodiscard]] const EditorShellState& state() const { return state_; }

protected:
  bool onInit() override;
  void onSaveLocationChosen(const std::filesystem::path& path) override;
  void onFolderChosen(const std::filesystem::path& path) override;
  [[nodiscard]] GuiColor frameClearColor() const override;
  [[nodiscard]] RhiTextureHandle sceneDepthTarget() override;
  void recordScene(RhiCommandList& cmd) override;
  bool onTick(float dt) override;
  void onShutdown() override;
  void onClientKeyDown(uint32_t key, ClientKeyDownKind kind,
                       ClientKeyModifiers modifiers) override;

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
  /// Create the asset browser and wire its drops back to this editor.
  void initAssetPanel(GuiWidgetTree& tree);
  /// Create the properties panel and wire its edits back to this editor.
  void initPropertiesPanel(GuiWidgetTree& tree);
  /// Rescan the open project's assets, rebuild the folder tree, and
  /// refresh the panel.
  void refreshAssets();
  /// Push the scanned assets and their folders into the browser.
  void refreshAssetPanel();
  /// Put what the browser dropped into the world, if it landed over the
  /// viewport. The entry is the browser's numbering: an asset, or one of
  /// the built-in items numbered after them.
  void dropBrowserEntry(size_t entry, float x, float y);
  /// Put the browser entry on @p tile, whichever list it belongs in.
  void placeBrowserEntry(size_t entry, WorldPoint tile);
  /// Put the asset at @p index on the tile at @p position, as an action the
  /// user can undo.
  void placeAsset(size_t index, WorldPoint position);
  /// Add the light a built-in item stands for, over the tile at @p tile, as
  /// an action the user can undo.
  void placeLight(EditorGeneralItem item, WorldPoint tile);
  /// Carry out the Edit menu's undo and redo. Returns false when the
  /// command belongs to another menu.
  bool runEditCommand(EditorMenuCommand command);
  /// Revert the newest action, carrying the selection with it.
  void runUndo();
  /// Reapply the oldest reverted action, carrying the selection with it.
  void runRedo();
  /// Run the selection accelerators. Returns true when @p key was one.
  bool handleSelectionKey(uint32_t key);
  /// Select a tool from a number key, if @p key is one of them.
  void handleToolKey(uint32_t key);
  /// Select @p selection, or nothing when it names an entry the document
  /// does not have.
  void select(EditorSelection selection);
  /// Select what a viewport pick reported: markers are the placements and
  /// then the lights, so which list a marker belongs to is which half of
  /// that run it falls in.
  void selectMarker(int marker);
  /// How many entries the list the selection names holds, and zero when
  /// nothing is selected — so `index >= selectionCount()` is the one test
  /// for "the selection names nothing that is there".
  [[nodiscard]] size_t selectionCount() const;
  /// Whether the entry at @p index of @p kind's list is the selected one.
  [[nodiscard]] bool isSelected(EditorSelectionKind kind, size_t index) const;
  /// Push the selection into the properties panel and the viewport.
  void applySelectionToChrome();
  /// Show the selected placement's asset name and transform in @p panel.
  void showPlacementSelection(EditorPropertiesWidget& panel);
  /// Show the selected light's kind and properties in @p panel.
  void showLightSelection(EditorPropertiesWidget& panel);
  /// Apply one property change to whatever is selected, recording history
  /// when the gesture that produced it has finished.
  void applyPropertyEdit(EditorPropertyField field, float value,
                         EditorPropertyEdit edit);
  /// Apply one property change to the selected placement.
  void applyPlacementEdit(EditorPropertyField field, float value,
                          EditorPropertyEdit edit);
  /// Apply one property change to the selected light.
  void applyLightEdit(EditorPropertyField field, float value,
                      EditorPropertyEdit edit);
  /// Finish whatever edit the panel had in flight, before anything other
  /// than that panel changes the document or the selection.
  void commitPendingEdit();
  /// Whether the entry a finished gesture belongs to is still selected and
  /// still there. An undo or a rescan between the last preview and the
  /// commit takes the subject away, and there is then nothing to record
  /// the gesture against.
  [[nodiscard]] bool editSubjectSelected(EditorSelectionKind kind) const;
  /// Carry out one action, record it, and show the result.
  void recordAction(const EditorAction& action);
  /// Record the finished placement edit as one undoable action.
  void commitPlacementEdit();
  /// Record the finished light edit as one undoable action.
  void commitLightEdit();
  /// Push a document change into the chrome: the viewport's placement
  /// markers, and whether the Edit menu's undo and redo rows are live.
  void applyEditToChrome();
  /// Load and upload an asset's mesh if it is not on the GPU yet. False
  /// when it cannot be loaded, which is remembered rather than retried.
  bool ensureAssetMesh(size_t index);
  /// Read, orient, and upload one asset's mesh.
  bool loadAssetMesh(EditorAsset& asset);
  /// Make pictures for a few of the cards on screen, and no more than a
  /// few: this runs every frame and must not stall one.
  void pumpThumbnails();
  /// Make pictures for grid slots `[first, last)`, up to the frame's
  /// budget. Returns how many were made.
  size_t pumpThumbnailRange(EditorAssetBrowserWidget& browser, size_t first,
                            size_t last);
  /// Make and upload one asset's card picture if it has none yet. False
  /// when it already has one, or when it has already failed.
  bool ensureAssetThumbnail(size_t index);
  /// The picture for an asset: the project's cached one, or a fresh render
  /// which is then cached. Empty when the mesh could not be read.
  [[nodiscard]] ImageData buildAssetThumbnail(const EditorAsset& asset);
  /// Upload a picture and hand the browser the texture. False when the
  /// device would not make one.
  bool uploadAssetThumbnail(EditorAsset& asset, const ImageData& image);
  /// Destroy every uploaded card picture. Called before the asset list is
  /// replaced, and again on shutdown.
  void releaseAssetThumbnails();
  /// The whole drawable surface as a GPU viewport.
  [[nodiscard]] RhiViewport surfaceViewport();
  /// Build the draw parameters for this frame's scene pass.
  [[nodiscard]] MeshRenderer::DrawParams
  sceneDrawParams(const EditorViewportWidget& viewport);
  /// Rebuild `scene_instances_` from the current placements.
  void buildSceneInstances();
  /// Rebuild `scene_lights_` from the document's lights.
  void buildSceneLights();
  /// Push placement and light boxes into the viewport for its overlay and
  /// picking, placements first.
  void refreshPlacementMarkers();
  /// The viewport's marker for the placement at @p index.
  [[nodiscard]] EditorPlacementMarker placementMarker(size_t index);
  /// The viewport's marker for the light at @p index: the small box that
  /// stands in for geometry a light does not have.
  [[nodiscard]] EditorPlacementMarker lightMarker(size_t index);
  /// Whether anything the chrome's layout depends on has changed.
  [[nodiscard]] bool chromeNeedsLayout();
  /// The viewport widget, or nullptr before the chrome exists.
  [[nodiscard]] EditorViewportWidget* viewportWidget();
  /// The asset browser, or nullptr before the chrome exists.
  [[nodiscard]] EditorAssetBrowserWidget* assetBrowserWidget();
  /// The properties panel, or nullptr before the chrome exists.
  [[nodiscard]] EditorPropertiesWidget* propertiesWidget();
  /// Width the properties panel wants, which is nothing without a
  /// selection.
  [[nodiscard]] float propertiesPanelWidth();
  /// Height the asset browser wants, which shrinks when it is folded away.
  [[nodiscard]] float assetBrowserHeight();
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
  /// Run the Edit accelerators — undo, and redo with Shift. Returns true
  /// when @p key with @p modifiers was one of them.
  bool handleEditKey(uint32_t key, ClientKeyModifiers modifiers);
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
  /// Properties panel widget id in the tree (owned by the tree).
  GuiWidgetId properties_panel_id_ = GUI_WIDGET_ID_INVALID;
  /// Mesh pipeline, uploaded meshes, and the scene depth target.
  MeshRenderer mesh_renderer_{};
  /// Instances rebuilt each frame from the placements. Kept as a member so
  /// a frame does not allocate.
  std::vector<MeshInstance> scene_instances_{};
  /// Lights rebuilt each frame from the document, in the renderer's own
  /// layout. A member for the same reason the instances are.
  std::vector<MeshLight> scene_lights_{};
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
  /// Asset browser height the chrome was laid out for. Folding the browser
  /// changes it, and the viewport above has to be given the difference.
  float laid_out_panel_height_ = ASSET_PANEL_HEIGHT;
  /// Properties panel width the chrome was laid out for. Selecting or
  /// deselecting changes it, and the viewport beside it takes the
  /// difference.
  float laid_out_properties_width_ = 0.0f;
  /// The selected placement as it was before the running property gesture,
  /// or nothing when no edit is in flight. This is the half of a transform
  /// action that undo restores.
  std::optional<EditorPlacement> placement_prior_{};
  /// The same for a light, since a gesture edits one or the other and the
  /// two are restored into different lists.
  std::optional<EditorLight> light_prior_{};
};

}  // namespace eng::editor
