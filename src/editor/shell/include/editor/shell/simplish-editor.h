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
//   - Lists the built-in general section above them, divided into
//     lighting, shapes and tools: its light sources drop into the world the
//     same way and light every mesh in the scene, its shapes are assets
//     whose geometry is generated rather than read from a file, and its
//     tools hold the player start, which marks where a player spawns and
//     is handed the lowest player that has no start yet
//   - Clicking a placed asset, a light or a player start selects it,
//     outlines it in the viewport, and opens a properties panel down the
//     right; the panel moves and turns a placement, aims, dims and tints a
//     light, and moves a player start or gives it to another player, and
//     clicking bare ground or pressing Escape puts the panel away again
//   - File > Save writes what has been placed and what lights it to the
//     project's own level file, and Ctrl/Cmd+S does the same; opening a
//     project reads that file back and puts everything in it back in the
//     viewport. While the level holds unwritten edits the project's name
//     carries a trailing asterisk, in the title bar and in the toolbar
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
//   - The Level menu lists the project's levels and marks the one being
//     edited; choosing another writes nothing, reads that level's file, and
//     replaces the document, the selection and the undo history with it.
//     Level > New Level asks for a name, turns it into an id, and writes an
//     empty level file before switching to it
//   - Runs an installed state hook once a tick with mutable shell state,
//     and rebuilds the chrome from it when the hook says it changed
//     something. This is how the agent API drives the editor without the
//     shell knowing an agent exists
//   - Mirrors the viewport's camera, hover and grid flag into shell state
//     each tick, so everything the editor knows is readable from one record
//   - Per frame: lays the chrome out for the current window size and pushes
//     hovered-tile and zoom into the toolbar status text
//
// Edge Cases:
//   - No project on the command line: the editor opens with no project and
//     the toolbar shows "No project". Close Project, Save and New Level are
//     the project-gated commands, and each is disabled until one is open
//   - Menu commands whose subsystem does not exist yet (save as, cut, copy,
//     paste, settings) are listed but disabled; see editor-menu-command.h
//   - A project with no level file yet — one nothing has been saved into —
//     opens with an empty document rather than an error
//   - A project holding no `main` level opens on the first level it does
//     hold, rather than on an empty one it does not
//   - Switching level with unwritten edits is refused with a status message
//     rather than silently losing them; the agent API has to ask for
//     "discard" in as many words
//   - A saved prop whose asset the project no longer holds is dropped on
//     load and counted in the log, for the reason a rescan drops one — and
//     the level counts as unsaved afterwards, since it no longer matches
//     the file it was read from
//   - A level file that is there but will not parse leaves the document
//     empty and refuses to be saved over, so a hand-edited typo costs a
//     session rather than the level
//   - A rescan renumbers the asset list, so it drops the document and the
//     history together: an action holding an old index would otherwise undo
//     into the new list, and the selection goes with them
//   - A scene with no lights in it is lit by the renderer's built-in key
//     light, so a project nobody has lit looks as it always did
//   - Undo and redo renumber one of the document's lists, so the selection
//     is moved with it rather than left pointing at whatever took the slot
//   - The state hook's chrome rebuild is gated on its answer: running it
//     every tick would rewrite the properties panel out from under a drag
//   - A placement made through the hook never went through a browser drag,
//     so the meshes of what has been placed are uploaded after it runs
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
//   - src/editor/agent/: installs the state hook and calls runMenuCommand,
//     openProjectAt, rescanAssets, createLevel and openLevel on behalf of
//     an agent

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <editor/project/project-open-error.h>
#include <editor/shell/editor-asset-browser-widget.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-character-figure.h>
#include <editor/shell/editor-dialog-purpose.h>
#include <editor/shell/editor-general-item.h>
#include <editor/shell/editor-level-result.h>
#include <editor/shell/editor-level-unsaved.h>
#include <editor/shell/editor-menu-bar-widget.h>
#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-placement-animator.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/editor-properties-widget.h>
#include <editor/shell/editor-property-edit.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-shell-state.h>
#include <editor/shell/editor-toolbar-widget.h>
#include <editor/shell/editor-viewport-widget.h>
#include <engine/client/desktop-game-client.h>
#include <engine/gltf/skinned-model.h>
#include <engine/gui/gui-widget-id.h>
#include <engine/gui/image-data.h>
#include <engine/input/held-actions.h>
#include <engine/math/vec2.h>
#include <engine/render-mesh/mesh-outline-renderer.h>
#include <engine/render-mesh/mesh-renderer.h>
#include <engine/render-mesh/mesh-style.h>
#include <engine/render-mesh/skinned-mesh-instance.h>
#include <engine/render-mesh/skinned-mesh-renderer.h>
#include <engine/sim/player-input.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

  /// Run @p hook once a tick, holding the shell state it may edit, before
  /// the chrome is rebuilt from that state.
  ///
  /// The one way in for a driver that is not a person at the window — the
  /// agent API in `src/editor/agent/` is what this exists for. It is
  /// deliberately blind to who is calling: the shell knows nothing about
  /// agents, sockets, or tools, and the hook is installed from outside.
  ///
  /// The hook returns whether it changed anything. That answer matters:
  /// rebuilding the chrome unconditionally every tick would reset the
  /// properties panel out from under a drag in progress.
  void setStateHook(std::function<bool(EditorShellState&)> hook);

  /// Carry out @p command exactly as choosing it from the menu bar would.
  ///
  /// Public because the camera and the project dialogs live behind it, and
  /// a caller outside the window has no other way to reach them. It does
  /// not check whether the command is enabled — `editorMenuCommandEnabled`
  /// is that question, and the menu bar asks it before it draws the row.
  void runMenuCommand(EditorMenuCommand command);

  /// Create the level @p id in the open project and edit it.
  ///
  /// Public for the reason `openProjectAt` is: the Level menu is not the
  /// only caller, and an agent has no other way in. @p unsaved decides what
  /// happens to edits the open level has not written.
  void createLevel(std::string_view id, EditorLevelUnsaved unsaved);

  /// Edit the project's level @p id instead of the open one.
  void openLevel(std::string_view id, EditorLevelUnsaved unsaved);

  /// Rescan the open project's assets from disk.
  ///
  /// Drops the document and the history with them, for the reason
  /// `refreshAssets` gives: a rescan renumbers the list they name.
  void rescanAssets();

protected:
  bool onInit() override;
  void onSaveLocationChosen(const std::filesystem::path& path) override;
  void onFolderChosen(const std::filesystem::path& path) override;
  [[nodiscard]] GuiColor frameClearColor() const override;
  [[nodiscard]] RhiTextureHandle sceneDepthTarget() override;
  void recordScene(RhiCommandList& cmd) override;
  void recordSceneOverlay(RhiCommandList& cmd) override;
  bool onTick(float dt) override;
  void onShutdown() override;
  void onClientKeyDown(uint32_t key, ClientKeyDownKind kind,
                       ClientKeyModifiers modifiers) override;
  void onClientKeyUp(uint32_t key) override;
  void onClientFocusLost() override;

private:
  // -- Playtest (simplish-editor-playtest.cpp) ------------------------------
  /// Whether the level is being played.
  [[nodiscard]] bool isPlaying() const;
  /// Start playing the open level, or stop playing it.
  void togglePlaytest();
  /// Build a playtest from the open level and start running it. Refused,
  /// with the reason in the status line, when no project is open.
  void startPlaytest();
  /// Where player 1 spawns when the level has no start for them: the tile
  /// under the middle of the viewport.
  [[nodiscard]] WorldPoint playtestFallback();
  /// Reset the shell's view of the playtest to the one that just started.
  void beginPlaytestState();
  /// Write the playtest's replay to the project's scratch data, logging
  /// rather than failing when it cannot be written.
  void saveLastPlaytestReplay();
  /// Throw the running playtest away and go back to editing. A no-op when
  /// nothing is being played, so every route that replaces the level can
  /// call it first.
  void stopPlaytest();
  /// Run the ticks this frame's time pays for, and publish what they did.
  void tickPlaytest();
  /// Nanoseconds of real time since the last frame of play.
  [[nodiscard]] uint64_t playtestElapsedNs();
  /// Player 1's input on the next tick, from the held keys, the left button
  /// and the cursor.
  [[nodiscard]] sim::PlayerInput livePlayerInput();
  /// The direction from player 1 to the world point under the cursor, or
  /// zero when the cursor is not over the viewport.
  [[nodiscard]] Vec2 cursorAim();
  /// Centre the viewport on player 1, where the frame draws them.
  void followPlayer();
  /// Add a column in its player's colour for every player, drawn where the
  /// frame puts them, after the level's own markers.
  void appendPlaytestMarkers(std::vector<EditorPlacementMarker>& markers);
  /// The characters the viewport draws this frame: every player while a
  /// playtest runs, and the ones standing on the level's starts otherwise.
  [[nodiscard]] std::vector<EditorCharacterFigure> characterFigures() const;
  /// Draw every character `characterFigures` lists.
  void appendCharacterInstances();
  /// Draw @p figure as its character, or as the stand-in when it has none
  /// or the asset will not load.
  void appendCharacterInstance(const EditorCharacterFigure& figure);
  /// Draw the stand-in — the built-in cylinder at a player's proportions —
  /// with its feet on @p feet.
  void appendStandIn(Vec3 feet);
  /// Draw @p figure as the rigged model @p asset, posed on the animation
  /// clock in the clip its gait picks.
  void appendSkinnedCharacter(const EditorCharacterFigure& figure,
                              size_t asset);
  /// The asset @p character references, uploaded; nothing for the stand-in
  /// and for one that is missing or will not load.
  [[nodiscard]] std::optional<size_t>
  characterAsset(const std::string& character);
  /// The built-in cylinder, which stands in for a player, uploaded; nothing
  /// when it is not in the asset list or will not load.
  [[nodiscard]] std::optional<size_t> avatarAsset();
  /// Start, stop, and steer a playtest from the keyboard. Returns true when
  /// @p key was one of the keys a playtest took.
  bool handlePlaytestKey(uint32_t key, ClientKeyDownKind kind);
  /// The keys a running playtest takes: Escape stops it, and the movement
  /// keys are held. Returns true when @p key was one of them.
  bool handlePlayingKey(uint32_t key);
  /// Every accelerator the editor has while editing.
  void handleEditingKey(uint32_t key, ClientKeyDownKind kind,
                        ClientKeyModifiers modifiers);
  /// Push the play mode into the toolbar's button and the Level menu.
  void applyPlayModeToChrome();
  /// The toolbar status line while playing: the tick, and any stutter.
  [[nodiscard]] std::string playtestStatus() const;

  /// Create the toolbar and wire its tool and play buttons.
  void initToolbar(GuiWidgetTree& tree);
  /// Let the menu bar open, close and rebuild its dropdowns for the frame.
  void tickMenuBar();
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
  /// Drop the document and load the newly-opened project's assets.
  void refreshAssets();
  /// Write the level to the open project, and say so in the status line.
  void saveDocument();
  /// Whether the level may be written right now, saying why in the status
  /// line when it may not.
  [[nodiscard]] bool canSaveDocument();
  /// Write the level file, logging and showing the reason when it fails.
  bool writeLevelFile();
  /// Remember and report a level file that is there but will not parse.
  void reportLevelUnreadable();
  /// Say how many props the level lost because their asset is gone.
  void reportDroppedProps(size_t dropped);
  /// Say how many entities the level lost because the editor has no
  /// definition for them.
  void reportDroppedEntities(size_t dropped);
  /// Show what a level operation did, and rebuild the chrome when it
  /// changed which level is open.
  void applyLevelResult(const EditorLevelResult& result, std::string_view id);
  /// Take a newly-opened level as the one on screen: its meshes, its
  /// markers, its name, and the gestures the old one had in flight.
  void adoptOpenedLevel(const EditorLevelResult& result);
  /// Read the open project's level back into the document, if it has one.
  /// Runs after the assets are scanned, because a prop names its asset by
  /// an id that only the scanned list can be searched for.
  void loadDocument();
  /// Rescan the assets directory and refresh the panel, leaving the
  /// document alone.
  void reloadAssets();
  /// Every asset's id, by the index it currently sits at.
  [[nodiscard]] std::vector<std::string> assetIds() const;
  /// Point every placement at its asset's new index, dropping the ones
  /// whose asset the rescan did not find again.
  void rebindPlacements(const std::vector<std::string>& previous_ids);
  /// Keep or drop the selection after a rebind, given how many placements
  /// the rebind took out from under it.
  void reselectAfterRescan(size_t dropped);
  /// Forget the level and the history describing it, which a rescan
  /// invalidates by renumbering the assets they name.
  void clearDocument();
  /// Take a scan as the asset list and the folder tree, with the built-in
  /// shapes on the end of the list and the general section above the
  /// scanned folders.
  void adoptAssetScan(EditorAssetScan scan);
  /// Push the scanned assets and their folders into the browser.
  void refreshAssetPanel();
  /// Put what the browser dropped into the world, if it landed over the
  /// viewport. The entry is the browser's numbering: an asset, or one of
  /// the built-in items numbered after them.
  void dropBrowserEntry(size_t entry, float x, float y);
  /// Put the browser entry on @p tile, whichever list it belongs in.
  void placeBrowserEntry(size_t entry, WorldPoint tile);
  /// Make asset @p entry the character of the player start under
  /// @p screen, as one undoable edit. False, changing nothing, when no
  /// start is there — the drop is then a placement like any other.
  bool dressPlayerStart(size_t entry, IsoPoint screen);
  /// Index of the player start whose marker is under @p screen, or nothing.
  [[nodiscard]] std::optional<size_t> playerStartUnder(IsoPoint screen);
  /// Put the asset at @p index on the tile at @p position, as an action the
  /// user can undo.
  void placeAsset(size_t index, WorldPoint position);
  /// Put the built-in item @p item on @p tile, whichever sort it is.
  void placeGeneralItem(EditorGeneralItem item, WorldPoint tile);
  /// Add a light of @p kind over the tile at @p tile, as an action the user
  /// can undo.
  void placeLight(EditorLightKind kind, WorldPoint tile);
  /// Add a player start on the tile at @p tile, for the lowest player that
  /// has none yet, as an action the user can undo.
  void placePlayerStart(WorldPoint tile);
  /// Carry out the Edit menu's undo, redo and delete. Returns false when
  /// the command belongs to another menu.
  bool runEditCommand(EditorMenuCommand command);
  /// Revert the newest action, carrying the selection with it.
  void runUndo();
  /// Reapply the oldest reverted action, carrying the selection with it.
  void runRedo();
  /// Remove what is selected, as an action the user can undo. Does nothing
  /// when the selection names nothing that is there.
  void runDelete();
  /// Run the selection accelerators — Escape to deselect, Backspace or
  /// Delete to remove. Returns true when @p key was one of them.
  bool handleSelectionKey(uint32_t key);
  /// Select a tool from a number key, if @p key is one of them.
  void handleToolKey(uint32_t key);
  /// Select @p selection, or nothing when it names an entry the document
  /// does not have.
  void select(EditorSelection selection);
  /// Select what a viewport pick reported: markers are the placements, then
  /// the lights, then the player starts, so which list a marker belongs to
  /// is which run it falls in.
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
  /// Show the selected player start's player, position and character in
  /// @p panel.
  void showPlayerStartSelection(EditorPropertiesWidget& panel);
  /// Apply one property change to whatever is selected, recording history
  /// when the gesture that produced it has finished.
  void applyPropertyEdit(EditorPropertyField field, float value,
                         EditorPropertyEdit edit);
  /// Apply one property change to the selected placement.
  void applyPlacementEdit(EditorPropertyField field, float value,
                          EditorPropertyEdit edit);
  /// Act on the properties panel's choice row picking @p index: a clip for
  /// a placement, a character for a player start.
  void applyChoiceEdit(size_t index);
  /// Give the selected player start the character at @p index of its
  /// Character row.
  void applyCharacterChoice(size_t index);
  /// Have the selected placement play the clip at @p index of its
  /// Animation row.
  void applyClipChoice(size_t index);
  /// Set the clip the selected placement plays, as one undoable edit —
  /// what the properties panel's Animation row reports.
  void applyClipEdit(const std::string& clip);
  /// Set the character the selected player start is drawn as, as one
  /// undoable edit — what the properties panel's Character row reports.
  void applyCharacterEdit(const std::string& character);
  /// Apply one property change to the selected light.
  void applyLightEdit(EditorPropertyField field, float value,
                      EditorPropertyEdit edit);
  /// Apply one property change to the selected player start.
  void applyPlayerStartEdit(EditorPropertyField field, float value,
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
  /// Record the finished player start edit as one undoable action.
  void commitPlayerStartEdit();
  /// Push a document change into the chrome: the viewport's placement
  /// markers, and whether the Edit menu's undo and redo rows are live.
  void applyEditToChrome();
  /// Load and upload an asset's mesh if it is not on the GPU yet. False
  /// when it cannot be loaded, which is remembered rather than retried.
  bool ensureAssetMesh(size_t index);
  /// Read, orient, and upload one asset's mesh, and its diffuse map with
  /// it.
  bool loadAssetMesh(EditorAsset& asset);
  /// `loadAssetMesh` for a rigged glTF model: upload its skinned mesh to
  /// `skinned_renderer_` and keep its rig on the asset. A file that will not
  /// load says why in the status bar, since the reason is one the user can
  /// act on in their exporter.
  bool loadRiggedAsset(EditorAsset& asset);
  /// Turn a loaded rigged model Z-up and make it @p asset's: its skinned
  /// mesh uploaded, its bounds and map taken, its rig kept.
  bool adoptRiggedModel(EditorAsset& asset, gltf::SkinnedModel& model);
  /// Decode and upload one image as a mesh texture. Invalid when there is
  /// no path, no device, or the file will not decode.
  [[nodiscard]] RhiTextureHandle
  uploadMeshTexture(const std::filesystem::path& path);
  /// Destroy every uploaded mesh texture. Called before the asset list is
  /// replaced, and again on shutdown.
  void releaseAssetTextures();
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
  /// Where the open project keeps thumbnails for its current projection.
  [[nodiscard]] std::filesystem::path thumbnailCacheDir() const;

  [[nodiscard]] ImageData buildAssetThumbnail(const EditorAsset& asset);

  /// Render or load the thumbnail for an asset that has a file behind it.
  [[nodiscard]] ImageData buildCachedThumbnail(const EditorAsset& asset);

  /// The axes thumbnails are drawn with: the open project's own.
  [[nodiscard]] IsoAxes thumbnailAxes() const;
  /// Upload a picture and hand the browser the texture. False when the
  /// device would not make one.
  bool uploadAssetThumbnail(EditorAsset& asset, const ImageData& image);
  /// Destroy every uploaded card picture. Called before the asset list is
  /// replaced, and again on shutdown.
  void releaseAssetThumbnails();
  /// The whole drawable surface as a GPU viewport.
  [[nodiscard]] RhiViewport surfaceViewport();
  /// Surface pixels per layout pixel — two on a Retina display.
  [[nodiscard]] float surfaceScale();
  /// Build the draw parameters for this frame's scene pass.
  [[nodiscard]] MeshRenderer::DrawParams
  sceneDrawParams(const EditorViewportWidget& viewport);
  /// Build the draw parameters for this frame's outline pass, which match
  /// the scene pass's so the line sits exactly on what it outlines.
  [[nodiscard]] MeshOutlineRenderer::DrawParams
  outlineDrawParams(const EditorViewportWidget& viewport);
  /// The style the open project's meshes are drawn with.
  [[nodiscard]] MeshStyle sceneStyle() const;
  /// Create the mesh and outline pipelines, warning about whichever the
  /// backend lacks. Neither is fatal: the editor runs without geometry.
  void initSceneRenderers();
  /// Rebuild `scene_instances_` and `skinned_instances_` from the current
  /// placements.
  void buildSceneInstances();
  /// Append @p placement's instance to whichever list its model draws in.
  void appendPlacementInstance(const EditorPlacement& placement);
  /// Pose @p placement's rigged model at the animation clock and append it
  /// to `skinned_instances_`.
  void appendSkinnedInstance(const EditorAsset& asset,
                             const EditorPlacement& placement);
  /// The static pass's parameters with the rigged instances in place of
  /// the static ones: same camera, lights, scissor and style.
  [[nodiscard]] SkinnedMeshRenderer::DrawParams
  skinnedDrawParams(const EditorViewportWidget& viewport);
  /// Rebuild `scene_lights_` from the document's lights.
  void buildSceneLights();
  /// Push placement, light and player start boxes into the viewport for
  /// its overlay and picking, in that order.
  void refreshPlacementMarkers();
  /// The viewport's marker for the placement at @p index.
  [[nodiscard]] EditorPlacementMarker placementMarker(size_t index);
  /// The viewport's marker for the light at @p index: the small box that
  /// stands in for geometry a light does not have.
  [[nodiscard]] EditorPlacementMarker lightMarker(size_t index);
  /// The viewport's marker for the player start at @p index: a column about
  /// a person tall, in its player's colour.
  [[nodiscard]] EditorPlacementMarker playerStartMarker(size_t index);
  /// Whether anything the chrome's layout depends on has changed.
  [[nodiscard]] bool chromeNeedsLayout();
  /// The viewport widget, or nullptr before the chrome exists.
  [[nodiscard]] EditorViewportWidget* viewportWidget();

  /// Apply one of the View menu's camera rows — reset, zoom, or the grid.
  void applyCameraCommand(EditorMenuCommand command);

  /// Switch the open project to @p projection and write the choice back to
  /// its project.json. A no-op with no project open, since there would be
  /// nowhere to record the choice.
  void applyProjection(ProjectProjection projection);

  /// Push the open project's projection into the viewport camera and the
  /// menu's checked row.
  void applyProjectionToWidgets();

  /// Switch the open project to @p shading and write the choice back to its
  /// project.json. Takes effect on the next frame; a no-op with no project
  /// open, since there would be nowhere to record the choice.
  void applyShading(ProjectShading shading);

  /// Push the open project's shading into the menu's checked row. The
  /// renderer needs no pushing: it reads the setting every frame.
  void applyShadingToWidgets();
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
  /// Put the project's name, and its unsaved asterisk, on the title bar,
  /// the window, and the toolbar — and touch nothing else.
  void applyProjectNameToChrome();
  /// Redraw that name when the level has gone from saved to unsaved, or
  /// back. A no-op on every other call, which is most of them.
  void refreshUnsavedMarker();
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
  /// Run the File accelerators — save, with Ctrl or Command. Returns true
  /// when @p key with @p modifiers was one of them.
  bool handleFileKey(uint32_t key, ClientKeyModifiers modifiers);
  /// Run the Edit accelerators — undo, and redo with Shift. Returns true
  /// when @p key with @p modifiers was one of them.
  bool handleEditKey(uint32_t key, ClientKeyModifiers modifiers);
  /// Copy the viewport's camera, hover, and grid flag into the shell
  /// state, so everything the editor knows is readable from one record.
  void syncViewState();
  /// Run the state hook, and rebuild the chrome when it changed something.
  void runStateHook();
  /// Upload the mesh of every asset something has placed and not yet
  /// loaded. A placement made through the agent API skips the browser's
  /// drag, which is where a drop would otherwise have uploaded it.
  void ensurePlacedMeshes();
  /// Let the chrome widgets release what they own, then destroy them.
  void shutdownChrome();
  /// Destroy the chrome nodes and forget their ids.
  void destroyChromeWidgets(GuiWidgetTree& tree);

  /// Persistent shell state.
  EditorShellState state_{};
  /// Run once a tick with that state, or empty when nothing installed one.
  std::function<bool(EditorShellState&)> state_hook_{};
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
  /// Outline pipeline, which reads `mesh_renderer_`'s depth target.
  MeshOutlineRenderer outline_renderer_{};
  /// Skinned-mesh pipeline and the rigged models' uploaded meshes. Draws
  /// in the same pass, against the same depth, as `mesh_renderer_`.
  SkinnedMeshRenderer skinned_renderer_{};
  /// Rigged placements, posed, rebuilt each frame as `scene_instances_` is.
  std::vector<SkinnedMeshInstance> skinned_instances_{};
  /// Every rigged placement's clip playback, kept between frames so that a
  /// change of clip fades. Each instance's skin span points into it.
  EditorPlacementAnimator placement_animator_{};
  /// Seconds every placed clip loops on. Presentation only: it runs off the
  /// frame's delta, and neither the level nor the simulation reads it.
  double animation_clock_ = 0.0;
  /// Instances rebuilt each frame from the placements. Kept as a member so
  /// a frame does not allocate.
  std::vector<MeshInstance> scene_instances_{};
  /// Lights rebuilt each frame from the document, in the renderer's own
  /// layout. A member for the same reason the instances are.
  std::vector<MeshLight> scene_lights_{};
  /// Backing store for the title label's string_view.
  std::string title_text_{"Simplish Editor"};
  /// Whether the name currently on screen carries the unsaved asterisk.
  bool shown_unsaved_ = false;
  /// Message shown in place of the toolbar status while it lasts.
  std::string status_override_{};
  /// Seconds `status_override_` still has to run.
  float status_override_left_ = 0.0f;
  /// What the open save-location dialog was asked for, which is what its
  /// answer is used as.
  EditorDialogPurpose pending_dialog_ = EditorDialogPurpose::NEW_PROJECT;
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
  /// The same for a player start.
  std::optional<EditorPlayerStart> player_start_prior_{};
  /// The level being played, or nothing while editing.
  std::unique_ptr<EditorPlaytestSession> playtest_{};
  /// When the last frame of play ran, for the playtest's clock.
  std::chrono::steady_clock::time_point playtest_frame_{};
  /// How far the last frame of play got between its two newest ticks,
  /// which is where the players are drawn.
  float playtest_alpha_ = 0.0f;
  /// The movement and fire keys held right now.
  input::HeldActions held_actions_{};
};

}  // namespace eng::editor
