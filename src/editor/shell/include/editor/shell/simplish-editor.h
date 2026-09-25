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
#include <editor/build/editor-build-command.h>
#include <editor/build/editor-build-job.h>
#include <editor/build/editor-build-kind.h>
#include <editor/build/editor-build-status.h>
#include <editor/build/editor-logic-library.h>
#include <editor/project/project-open-error.h>
#include <editor/shell/editor-asset-browser-widget.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-character-figure.h>
#include <editor/shell/editor-character-select-widget.h>
#include <editor/shell/editor-choice-kind.h>
#include <editor/shell/editor-clip-pass.h>
#include <editor/shell/editor-controls-row.h>
#include <editor/shell/editor-controls-widget.h>
#include <editor/shell/editor-dialog-purpose.h>
#include <editor/shell/editor-effect-shot.h>
#include <editor/shell/editor-emitter-player.h>
#include <editor/shell/editor-event-hit.h>
#include <editor/shell/editor-general-item.h>
#include <editor/shell/editor-ground-brush.h>
#include <editor/shell/editor-ground-ops.h>
#include <editor/shell/editor-level-result.h>
#include <editor/shell/editor-level-unsaved.h>
#include <editor/shell/editor-menu-bar-widget.h>
#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-placement-animator.h>
#include <editor/shell/editor-playtest-controls.h>
#include <editor/shell/editor-playtest-run.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/editor-properties-widget.h>
#include <editor/shell/editor-property-edit.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-shell-state.h>
#include <editor/shell/editor-sound-import-result.h>
#include <editor/shell/editor-sound-row.h>
#include <editor/shell/editor-sound-widget.h>
#include <editor/shell/editor-sprite-ops.h>
#include <editor/shell/editor-sprite-quad-key.h>
#include <editor/shell/editor-sprite-sheet-texture.h>
#include <editor/shell/editor-stroke-phase.h>
#include <editor/shell/editor-toolbar-widget.h>
#include <editor/shell/editor-viewport-widget.h>
#include <editor/shell/editor-water-depths.h>
#include <editor/shell/editor-water.h>
#include <engine/client/desktop-game-client.h>
#include <engine/gltf/skinned-model.h>
#include <engine/gui/gui-widget-id.h>
#include <engine/gui/image-data.h>
#include <engine/input/gamepad-seats.h>
#include <engine/input/held-actions.h>
#include <engine/input/input-bindings.h>
#include <engine/math/vec2.h>
#include <engine/render-fx/fx-renderer.h>
#include <engine/render-fx/fx-volume-renderer.h>
#include <engine/render-fx/fx-world.h>
#include <engine/render-ground/ground-grid.h>
#include <engine/render-mesh/mesh-outline-renderer.h>
#include <engine/render-mesh/mesh-renderer.h>
#include <engine/render-mesh/mesh-style.h>
#include <engine/render-mesh/skinned-mesh-instance.h>
#include <engine/render-mesh/skinned-mesh-renderer.h>
#include <engine/render-water/water-renderer.h>
#include <engine/sim/player-input.h>
#include <filesystem>
#include <functional>
#include <game/content/faction.h>
#include <game/fx/combat-sounds.h>
#include <game/fx/footstep-sounds.h>
#include <map>
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

  /// Read the user's control scheme from @p path — writing the defaults
  /// there when it has none — and play with it from now on. Without a
  /// call, a playtest plays with the defaults and nothing is saved.
  void setInputBindingsPath(const std::filesystem::path& path);

  /// Read the user's volume settings from @p path — writing full volume
  /// there when it has none — and play at them from now on. Without a
  /// call, the editor plays at full volume and nothing is saved.
  void setAudioVolumesPath(const std::filesystem::path& path);

  /// Read the user's graphics settings from @p path — writing the defaults
  /// there when it has none — and draw with them from now on. Without a
  /// call, the editor draws at the defaults and nothing is saved.
  void setGraphicsSettingsPath(const std::filesystem::path& path);

  /// Play what @p name names once, flat, through the effects bus: a sound
  /// slot (`combat.blast`, or `blast`) as the game plays it now, or a sound
  /// file under the project's `assets/`. False when it names neither, or
  /// the file cannot be played.
  bool previewSound(std::string_view name);

  /// Bring the sound file at @p source into the open project — copied into
  /// `assets/sounds/` unless it is already under `assets/` — and list it,
  /// saying what happened in the status line. With @p slot, also play it
  /// there from now on.
  EditorSoundImport importSoundFile(const std::filesystem::path& source,
                                    std::string_view slot);

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

  /// Play the open level with player 1 as the character @p character — an
  /// id in the characters table, or empty for the default character —
  /// closing the selector if it is up. What picking a card does, and how an
  /// agent starts a playtest without one.
  void startPlaytestAs(const std::string& character);

  /// Pause a running playtest, or resume a paused one. What F6 and Level ›
  /// Pause Playtest do.
  void togglePlaytestPause();

  /// Pause a running playtest and run exactly @p ticks ticks of it, on the
  /// input queued for player 1 or held on the keyboard — what F7 does for
  /// one tick, and how an agent reaches an exact tick. A no-op when no
  /// playtest is running.
  void stepPlaytest(uint32_t ticks);

  /// Play @p shot once, now, into the effects the viewport is drawing: the
  /// editor's own while the level is edited, the playtest's while it is
  /// played. Presentation — nothing records it, and a paused playtest
  /// holds it where it starts until it is stepped or resumed.
  void playEffectShot(const EditorEffectShot& shot);

protected:
  bool onInit() override;
  void onSaveLocationChosen(const std::filesystem::path& path) override;
  void onFolderChosen(const std::filesystem::path& path) override;
  void onSoundFileChosen(const std::filesystem::path& path) override;
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
  void onClientGamepadButtonDown(input::GamepadButton button) override;

private:
  // -- Build (simplish-editor-build.cpp) ------------------------------------
  /// Carry out @p command when it is one of the Build menu's. Returns true
  /// when it was.
  bool runBuildCommand(EditorMenuCommand command);
  /// Give the open project game logic of its own to start from.
  void newGameLogic();
  /// Build the open project's game logic in the background — scaffolding
  /// it first when it has none — for the next playtest to run.
  void buildGameLogic();
  /// Bake the open project's levels and content into its deploy folder,
  /// then build the deployed game in the background.
  void deployGame();
  /// Start a build of @p kind running @p commands. False, with the reason
  /// in the status line, when one is already running.
  bool startBuild(EditorBuildKind kind,
                  std::vector<EditorBuildCommand> commands);
  /// Once a tick: follow the open project, and finish a build that has
  /// ended.
  void tickBuild();
  /// When the open project is not the one the build state describes, drop
  /// the old one's library and load this one's, if it has one built.
  void followBuildProject();
  /// Drop the loaded logic library and what the build state says of it
  /// and of the last deploy: they describe a project no longer open.
  void forgetGameLogic();
  /// Everything that follows a build ending as @p status.
  void finishBuild(EditorBuildStatus status);
  /// What follows a logic build ending: load what it made, if it made
  /// something, and say how it went.
  void finishLogicBuild();
  /// Load the project's built logic library for the next playtest.
  void loadGameLogic();
  /// Read again whether the project has logic, and whether the library is
  /// older than its source.
  void refreshLogicState();
  /// Write every level's setup, the content tables and the manifest into
  /// the deploy folder. False, with the reason shown, when it could not.
  bool bakeDeployContent();
  /// Bake every saved level's setup into @p content's `levels/`, returning
  /// the ids of those baked, in the project's order.
  [[nodiscard]] std::vector<std::string>
  bakeDeployLevels(const std::filesystem::path& content);
  /// The setup the saved level @p id starts a run from, or nothing when
  /// its file will not read.
  [[nodiscard]] std::optional<game::GameSetup>
  bakeLevelSetup(const std::string& id);
  /// Copy the built game beside its content. False when it is not there.
  bool finishDeploy();
  /// What the next playtest is of, and the logic it runs — reading again,
  /// as it does, whether that logic is older than its source, so Play can
  /// say so.
  [[nodiscard]] EditorPlaytestRun playtestRun();
  /// What the status line adds about the project's logic when play
  /// starts: that it is not built, or older than its source; else nothing.
  [[nodiscard]] std::string playtestLogicNote() const;

  // -- Playtest (simplish-editor-playtest.cpp) ------------------------------
  /// Whether the level is being played.
  [[nodiscard]] bool isPlaying() const;
  /// Start playing the open level — by way of the selector, when there is a
  /// choice to make — or stop playing it, or put the selector away.
  void togglePlaytest();
  /// What Play does: read the characters again, then open the selector
  /// when there are two or more of them to pick between, and otherwise play
  /// at once as the one there is, or the default. Refused, with the reason
  /// in the status line, when no project is open.
  void requestPlaytest();
  /// Show the selector over the viewport, on the character player 1 would
  /// play as without a pick.
  void openCharacterSelect();
  /// Put the selector away without playing. A no-op when it is not up.
  void closeCharacterSelect();
  /// The selector's widget, or null before the chrome exists.
  [[nodiscard]] EditorCharacterSelectWidget* characterSelectWidget();
  /// Act on a key while the selector is up: arrows move the highlight,
  /// Enter plays, Esc cancels. Returns true when it took the key.
  bool handleChoosingKey(uint32_t key);
  /// Read the project's characters table into the state, logging what was
  /// wrong with it.
  void reloadCharacters();
  /// Read the project's behaviors table into the state, logging what was
  /// wrong with it.
  void reloadBehaviors();
  /// Read the project's enemies table into the state, logging what was
  /// wrong with it.
  void reloadEnemies();
  /// Read every data table the project has — characters, behaviors,
  /// enemies — into the state.
  void reloadDataTables();
  /// What a playtest is played with: the project's characters and
  /// behaviors, as last read.
  [[nodiscard]] game::GameContent playtestContent() const;
  /// Where player 1 spawns when the level has no start for them: the tile
  /// under the middle of the viewport.
  [[nodiscard]] WorldPoint playtestFallback();
  /// The status line while playing: the level, and who player 1 is.
  [[nodiscard]] std::string playingMessage() const;
  /// Reset the shell's view of the playtest to the one that just started.
  void beginPlaytestState();
  /// Write the playtest's replay to the project's scratch data, logging
  /// rather than failing when it cannot be written.
  void saveLastPlaytestReplay();
  /// Throw the running playtest away — or put the selector away — and go
  /// back to editing. A no-op when neither is up, so every route that
  /// replaces the level can call it first.
  void stopPlaytest();
  /// Run the ticks this frame's time pays for, and publish what they did.
  void tickPlaytest();
  /// Have the next playtest add @p stand_ins stand-in players, 0 to 3.
  void setStandIns(uint8_t stand_ins);
  /// Publish what the last ticks did, follow the player, and move the
  /// markers: everything after ticks run, stepped or on time.
  void afterPlaytestTicks();
  /// Nanoseconds of real time since the last frame of play.
  [[nodiscard]] uint64_t playtestElapsedNs();
  /// Player 1's input on the next tick, from the held keys, the left button
  /// and the cursor.
  [[nodiscard]] sim::PlayerInput livePlayerInput();
  /// Add players 2 to 4 to @p setup: one for every seated pad, and
  /// stand-ins up to the number the Level menu asks for.
  void addPlaytestPlayers(game::GameSetup& setup) const;
  /// Hand each pad seated for players 2 to 4 its player's input for the
  /// ticks this frame runs, and give a player whose pad went back to the
  /// stand-in.
  void feedPadPlayers();
  /// The pad in player 1's seat, or null.
  [[nodiscard]] const input::GamepadState* playerOnePad() const;
  /// The direction from player 1 to the world point under the cursor, or
  /// zero when the cursor is not over the viewport.
  [[nodiscard]] Vec2 cursorAim();
  /// Centre the viewport on player 1, where the frame draws them.
  void followPlayer();
  /// Move the ears to player 1 and play every cue the last ticks left
  /// worth hearing.
  void hearPlaytest();
  /// Where the playtest is heard from: player 1, where the frame draws
  /// them, with the screen's right as the viewport's camera turns it.
  [[nodiscard]] audio::AudioListener playtestListener();
  /// Add a column in its player's colour for every player, drawn where the
  /// frame puts them, after the level's own markers.
  void appendPlaytestMarkers(std::vector<EditorPlacementMarker>& markers);
  /// Push a marker for every projectile in flight and every hazard pool on
  /// the floor into @p markers, as the playtest last reported them.
  void appendCombatMarkers(std::vector<EditorPlacementMarker>& markers) const;
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
  /// The asset @p model references, uploaded; nothing for the stand-in and
  /// for one that is missing or will not load.
  [[nodiscard]] std::optional<size_t> characterAsset(const std::string& model);
  /// The built-in cylinder, which stands in for a player, uploaded; nothing
  /// when it is not in the asset list or will not load.
  [[nodiscard]] std::optional<size_t> avatarAsset();
  /// Start, stop, and steer a playtest from the keyboard. Returns true when
  /// @p key was one of the keys a playtest took.
  bool handlePlaytestKey(uint32_t key, ClientKeyDownKind kind);
  /// F6 and F7 while playing: pause or resume, and step one tick. Returns
  /// true when @p key was one of them.
  bool handleClockKey(uint32_t key, ClientKeyDownKind kind);
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
  /// How player 1 is doing, for the status line: their health, that they
  /// are down, or that the run is over.
  [[nodiscard]] std::string playerOneHealth() const;

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
  /// Create the character selector, hidden, over everything else in the
  /// work area, and wire its pick and cancel back to this editor.
  void initCharacterSelect(GuiWidgetTree& tree);

  // -- Controls screen (simplish-editor-controls.cpp) ------------------------
  /// Lay the screens that cover the viewport — the character selector and
  /// the Controls screen — over @p viewport.
  void layoutOverlays(GuiWidgetTree& tree, const Rect& viewport);
  /// Move the clips, and the edit-time effects, on by @p dt seconds.
  void tickPresentation(float dt);
  /// Build the Controls screen, hidden, over the viewport.
  void initControls(GuiWidgetTree& tree);
  /// The Controls screen, or null before the chrome exists.
  EditorControlsWidget* controlsWidget();
  /// Open the Controls screen; refused while a playtest runs or a character
  /// is being chosen, which own the keys.
  void openControls();
  /// Each action's row, labelled for the pad in use.
  [[nodiscard]] std::vector<EditorControlsRow> controlsRows() const;
  /// Every key while the Controls screen is open: it is modal. Returns
  /// whether it was open.
  bool handleControlsKey(uint32_t key, ClientKeyDownKind kind);
  /// The keys the screen takes while it is choosing a row.
  void handleControlsBrowseKey(uint32_t key);
  /// A pad button while the Controls screen is open; whether it was.
  bool handleControlsButton(input::GamepadButton button);
  /// A pad's menu @p command on @p screen while it is choosing a row.
  void browseControls(EditorControlsWidget& screen, GuiNavCommand command);
  /// While the Controls screen listens, bind a stick or trigger pushed past
  /// most of its travel on the pad in use.
  void listenForAxis();
  /// Put every action back on its defaults, keeping the deadzones.
  void resetControls();
  /// Bind @p source to the highlighted row's action, as a rebind does.
  void bindListened(input::InputSource source);
  /// Record a change to the controls: bump their revision and show it.
  void controlsChanged();
  /// Per frame: seat pads that were picked up, listen for a stick or
  /// trigger pushed while listening, and save the controls when they
  /// changed.
  void tickControls();
  /// Open the Controls or Sound screen, or ask for a sound to import;
  /// false for any other command.
  bool runSettingsCommand(EditorMenuCommand command);
  /// Build the Sound screen, hidden, over the viewport.
  void initSound(GuiWidgetTree& tree);
  /// The Sound screen, or null before the chrome exists.
  EditorSoundWidget* soundWidget();
  /// Open the Sound screen; refused while a character is being chosen.
  void openSound();
  /// The Sound screen's rows: the volumes, then the project's sounds.
  [[nodiscard]] std::vector<EditorSoundRow> soundRows() const;
  /// The volume rows and the mute, appended to @p rows.
  void appendVolumeRows(std::vector<EditorSoundRow>& rows) const;
  /// A row for each sound slot and the file it plays, appended to @p rows.
  void appendSlotRows(std::vector<EditorSoundRow>& rows) const;
  /// Every key while the Sound screen is open: it is modal. Returns whether
  /// it was open.
  bool handleSoundKey(uint32_t key, ClientKeyDownKind kind);
  /// The keys that act on the highlighted row, other than moving.
  void handleSoundRowKey(uint32_t key);
  /// A pad button while the Sound screen is open; whether it was.
  bool handleSoundButton(input::GamepadButton button);
  /// A pad's menu @p command on @p screen.
  void browseSound(EditorSoundWidget& screen, GuiNavCommand command);
  /// Left or right on the highlighted row: turn a volume, switch the mute,
  /// or step a slot through the project's files.
  void stepSoundRow(int steps);
  /// Enter on the highlighted row: switch the mute, or play a slot.
  void confirmSoundRow();
  /// Delete on the highlighted row: a slot goes back to its built-in sound.
  void clearSoundRow();
  /// Set volume row @p row to @p level, 0 to 1 — a click on its bar.
  void setSoundLevel(size_t row, float level);
  /// Mute, or unmute.
  void toggleMute();
  /// The highlighted row, or null when the screen is shut.
  [[nodiscard]] const EditorSoundRow* highlightedSoundRow();
  /// Ask for a sound file to import, into the highlighted slot if the
  /// Sound screen has one.
  void importSound();
  /// Per frame: apply and save the volumes, and save the sounds table and
  /// reload the clips, when either changed — here or by an agent.
  void tickSound();
  /// Write the sounds table after a change, then reload it.
  void saveSoundTable();
  /// Read the project's sounds table and load its files over the built-in
  /// sounds; logged, as the other tables are.
  void reloadSounds();
  /// Load the clips the sounds table names over the built-in sounds.
  void loadSoundClips();
  /// Step @p slot @p steps along the project's sound files.
  void stepSoundSlot(const std::string& slot, int steps);
  /// Play the project file @p file, under `assets/`, once.
  bool previewSoundFile(std::string_view file);
  /// Play @p file in @p slot from now on, when there is a slot.
  void assignImportedSound(std::string_view slot,
                           const std::filesystem::path& file);
  /// Rescan the project's sound files alone — after an import.
  void rescanSoundFiles();
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
  /// Add a waypoint on @p tile: to the route of the selected waypoint when
  /// one is selected — so a route is laid out by dropping one after
  /// another — and to route 1 otherwise, after its last waypoint.
  void placeWaypoint(WorldPoint tile);
  /// Add a particle emitter over @p tile, started from the default preset,
  /// as an action the user can undo, and select it.
  void placeEmitter(WorldPoint tile);
  /// Add a sprite billboard on @p tile, showing the project's first sprite
  /// sheet, as an action the user can undo, and select it.
  void placeSprite(WorldPoint tile);
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
  /// Show the selected waypoint's route, place and position in @p panel.
  void showWaypointSelection(EditorPropertiesWidget& panel);
  /// Show the selected emitter's Effect row and every number of its burst
  /// in @p panel.
  void showEmitterSelection(EditorPropertiesWidget& panel);
  /// Show the selected billboard's Sheet row and its numbers in @p panel.
  void showSpriteSelection(EditorPropertiesWidget& panel);
  /// Show whatever is selected — which is there — in @p panel.
  void showSelection(EditorPropertiesWidget& panel);
  /// Apply one property change to whatever is selected, which is there.
  void applySelectedEdit(EditorPropertyField field, float value,
                         EditorPropertyEdit edit);
  /// Apply one property change to whatever is selected, recording history
  /// when the gesture that produced it has finished.
  void applyPropertyEdit(EditorPropertyField field, float value,
                         EditorPropertyEdit edit);
  /// Apply one property change to the selected placement.
  void applyPlacementEdit(EditorPropertyField field, float value,
                          EditorPropertyEdit edit);
  /// Act on the properties panel's @p kind row picking @p index: a clip or
  /// a behavior or a faction for a placement, a character for a player
  /// start.
  void applyChoiceEdit(EditorChoiceKind kind, size_t index);
  /// The rows that make a placement an actor: its Behavior, Faction and
  /// Route rows picking @p index.
  void applyActorChoice(EditorChoiceKind kind, size_t index);
  /// Give the selected placement the behavior at @p index of its Behavior
  /// row.
  void applyBehaviorChoice(size_t index);
  /// Put the selected placement on the side at @p index of its Faction row.
  void applyFactionChoice(size_t index);
  /// Set the behavior and faction of the selected placement, as one
  /// undoable edit — what the Behavior and Faction rows report.
  void applyActorEdit(const std::string& behavior, game::Faction faction);
  /// Offer the selected placement's Behavior row, and its Faction row when
  /// it has a behavior, in @p panel.
  void showActorChoices(EditorPropertiesWidget& panel,
                        const EditorPlacement& placement) const;
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
  /// Record the waypoint gesture in flight, if it changed anything.
  void commitWaypointEdit();
  /// Apply one property change to the selected waypoint.
  void applyWaypointEdit(EditorPropertyField field, float value,
                         EditorPropertyEdit edit);
  /// Record the emitter gesture in flight, if it changed anything.
  void commitEmitterEdit();
  /// Apply one property change to the selected emitter.
  void applyEmitterEdit(EditorPropertyField field, float value,
                        EditorPropertyEdit edit);
  /// Start the selected emitter from the preset at @p index of its Effect
  /// row, as one undoable edit.
  void applyEffectChoice(size_t index);
  /// Record the billboard gesture in flight, if it changed anything.
  void commitSpriteEdit();
  /// Apply one property change to the selected billboard.
  void applySpriteEdit(EditorPropertyField field, float value,
                       EditorPropertyEdit edit);
  /// Point the selected billboard at the sheet at @p index of its Sheet
  /// row, as one undoable edit.
  void applySheetChoice(size_t index);
  /// Have the selected placement patrol the route at @p index of its Route
  /// row.
  void applyRouteChoice(size_t index);
  /// Apply a choice row that names one thing about the selected entry
  /// rather than one of the three an actor's rows share.
  void applyEntryChoice(EditorChoiceKind kind, size_t index);
  /// Pick @p index of the selected prop's Surface row or its Footsteps
  /// row, as one undoable edit.
  void applyFootstepChoice(EditorChoiceKind kind, size_t index);
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

  /// The axes the open project's projection draws with: what thumbnails
  /// are rendered at, and what a billboard is turned to face.
  [[nodiscard]] IsoAxes projectionAxes() const;
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
  /// Build the draw parameters for this frame's effects, which match the
  /// scene pass's so a spark sits where the thing it flew off is drawn.
  [[nodiscard]] FxRenderer::DrawParams
  fxDrawParams(const EditorViewportWidget& viewport);
  /// The same parameters again for the clouds of smoke, which are marched
  /// against the same depth before the particles are drawn over them.
  [[nodiscard]] FxVolumeRenderer::DrawParams
  fxVolumeDrawParams(const EditorViewportWidget& viewport);
  /// Draw the playtest's effects over the finished scene. Nothing while
  /// editing.
  void recordEffects(RhiCommandList& cmd, const EditorViewportWidget& viewport);
  /// Add the playtest's brightest flashes to `scene_lights_`, in whatever
  /// slots the level's own lights leave.
  void appendEffectLights();
  /// The style the open project's meshes are drawn with.
  [[nodiscard]] MeshStyle sceneStyle() const;
  /// Create the mesh and outline pipelines, warning about whichever the
  /// backend lacks. Neither is fatal: the editor runs without geometry.
  void initSceneRenderers();
  /// Create the pipelines the mesh pipeline can do without — outlines,
  /// skinned meshes and effects — warning about whichever the backend
  /// lacks.
  void initOptionalSceneRenderers(RhiDevice& device);
  /// Rebuild `scene_instances_` and `skinned_instances_` from the current
  /// placements.
  void buildSceneInstances();
  /// Append @p placement's instance to whichever list its model draws in.
  void appendPlacementInstance(const EditorPlacement& placement);
  /// Append the instance of the placement at @p index, the @p actor-th
  /// actor: where the game has it while playing, playing the clip its
  /// state and its gait call for.
  void appendActorInstance(size_t index, size_t actor);
  /// The placement at @p index, the @p actor-th actor, where it is drawn
  /// this frame: posed where the game has it while playing, as placed
  /// otherwise.
  [[nodiscard]] EditorPlacement posedActor(size_t index, size_t actor) const;
  /// The clip @p placement, the @p actor-th actor, plays in a playtest:
  /// its state's, when its model has that clip, or its walk or idle clip.
  [[nodiscard]] std::string actorClip(const EditorPlacement& placement,
                                      size_t actor) const;
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
  /// Push placement, light, player start and waypoint boxes into the
  /// viewport for its overlay and picking, in that order, and the routes
  /// the waypoints lay out.
  void refreshPlacementMarkers();
  /// Bring the navigation and AI overlays up to date with the level and
  /// the playtest, as far as each is on.
  void refreshOverlays();
  /// Measure the level's navigation again for the overlay while it is on,
  /// and clear it while it is off.
  void refreshNavigationOverlay();
  /// Say in the status line which actors cannot reach a player start,
  /// while the navigation overlay is on.
  void reportNavigation();
  /// Copy every actor's mind out of the running game for the AI overlay
  /// while it is on, and clear it while it is off or nothing is playing.
  void refreshActorOverlays();
  /// Flip the overlay @p command names on @p viewport, and bring it up to
  /// date.
  void toggleOverlay(EditorViewportWidget& viewport, EditorMenuCommand command);
  /// Push a marker for every placement into @p markers, in document order.
  void appendPlacementMarkers(std::vector<EditorPlacementMarker>& markers);
  /// Push a marker for every light, then every player start, then every
  /// waypoint into @p markers, each in document order.
  void appendEntityMarkers(std::vector<EditorPlacementMarker>& markers);
  /// The viewport's marker for the placement at @p index.
  [[nodiscard]] EditorPlacementMarker placementMarker(size_t index);
  /// The viewport's marker for the placement at @p index, the @p actor-th
  /// actor: its footprint in its faction's colour, facing its way, where
  /// it is drawn this frame.
  [[nodiscard]] EditorPlacementMarker actorMarker(size_t index, size_t actor);
  /// The viewport's marker for the light at @p index: the small box that
  /// stands in for geometry a light does not have.
  [[nodiscard]] EditorPlacementMarker lightMarker(size_t index);
  /// The viewport's marker for the player start at @p index: a column about
  /// a person tall, in its player's colour.
  [[nodiscard]] EditorPlacementMarker playerStartMarker(size_t index);
  /// The viewport's marker for the waypoint at @p index: a short post in
  /// its route's colour.
  [[nodiscard]] EditorPlacementMarker waypointMarker(size_t index);
  /// The viewport's marker for the emitter at @p index: a small box where
  /// its bursts start, in violet.
  [[nodiscard]] EditorPlacementMarker emitterMarker(size_t index);
  /// The viewport's marker for the billboard at @p index: the box it
  /// stands in, in teal.
  [[nodiscard]] EditorPlacementMarker spriteMarker(size_t index);
  /// How wide @p sprite is drawn, in tiles. Its own height until its sheet
  /// has loaded, which is the square a billboard with no sheet stands as.
  [[nodiscard]] float spriteWidth(const EditorSprite& sprite);
  /// Add an instance for every billboard in the level, each showing the
  /// frame the render clock has reached.
  void appendSpriteInstances();
  /// Add the instance for one billboard, if its sheet and its quad are
  /// there to draw with.
  void appendSpriteInstance(const EditorSprite& sprite);
  /// The sheet at @p path, uploading it on first use. Null when the path
  /// is empty, there is no device, or the image would not load.
  [[nodiscard]] const EditorSpriteSheetTexture*
  ensureSpriteSheet(const std::string& path);
  /// Read the image at @p path and remember it, whether or not it loaded.
  /// Null when it did not, which is what stops it being read again.
  const EditorSpriteSheetTexture* loadSpriteSheet(const std::string& path);
  /// The quad @p key names, uploading it on first use.
  [[nodiscard]] MeshGpuId ensureSpriteQuad(const EditorSpriteQuadKey& key);
  /// Upload the quad @p key names and keep it. Invalid when the renderer
  /// has no pipeline, or the buffers could not be allocated.
  [[nodiscard]] MeshGpuId uploadSpriteQuad(const EditorSpriteQuadKey& key);
  /// Release every billboard quad, leaving the sheet textures alone.
  void releaseSpriteQuads();
  /// Destroy every uploaded sheet texture and every billboard quad — for a
  /// project being closed, or a cache that has outgrown its bound.
  void releaseSpriteCache();
  /// Read the animation events table again, work out every loaded rig's
  /// clip events from it, and load the sound files it names.
  void reloadAnimationEvents();
  /// Every event the clips @p passes played reached, then every event the
  /// level's billboards reached over @p sprite_window of the sprite clock.
  [[nodiscard]] std::vector<EditorEventHit>
  eventHits(const std::vector<EditorClipPass>& passes,
            animation::ClipWindow sprite_window) const;
  /// Load the sound files the animation events table names, and log what
  /// could not be.
  void loadAnimationEventSounds();
  /// Add every actor to @p walkers, by its prop's id.
  void
  addActorWalkers(std::map<std::string, game::FootstepWalker>& walkers) const;
  /// Work out the events every loaded rig's clips play, from the table.
  void refreshClipEvents();
  /// Save the animation events table when it has changed, and reload it.
  void tickAnimationEvents();
  /// Hear what the animations drawn this frame reached, while playing:
  /// every clip's and every sprite sheet's events, each footstep through
  /// the playtest and every other sound straight to the speakers.
  void hearAnimationEvents();
  /// Every player and actor in the playtest, by the key its animation goes
  /// by (`player:1`, an actor prop's id), each with the key the playtest
  /// follows it by and its feet. Built once a frame.
  [[nodiscard]] std::map<std::string, game::FootstepWalker>
  frameWalkers() const;
  /// Play @p hits, of a playtest frame, footsteps as @p walkers says whose
  /// they are.
  void
  playEventHits(const std::vector<EditorEventHit>& hits,
                const std::map<std::string, game::FootstepWalker>& walkers);
  /// Play the sound @p hit names where it happened; false when there is no
  /// such sound, or no voice would take it.
  bool playEventSound(const EditorEventHit& hit);
  /// Save whichever of the project's editable tables — sounds, animation
  /// events — have changed since last saved.
  void tickTables();
  /// A footstep event of the animation @p hit belongs to, as the walker
  /// @p walkers knows it by — or, for a prop that walks no playtest, its
  /// own feet under no walker.
  [[nodiscard]] game::FootstepWalker
  eventWalker(const EditorEventHit& hit,
              const std::map<std::string, game::FootstepWalker>& walkers) const;
  /// Hand the walkers whose clips step for them to the playtest, from
  /// @p passes, so the stride stops stepping for them.
  void noteAnimatedWalkers(
      const std::vector<EditorClipPass>& passes,
      const std::map<std::string, game::FootstepWalker>& walkers);
  /// Select the painted area under the cursor, or clear the selection when
  /// the cursor is over bare ground — what a click on nothing does.
  void selectGroundUnderCursor();
  /// Show the selected area of ground: its terrain and size, and a Terrain
  /// row to repaint it with.
  void showGroundSelection(EditorPropertiesWidget& panel);
  /// Paint every cell of the selected area of ground with @p terrain, as
  /// one undoable edit; 0 erases it and drops the selection.
  void repaintSelectedGround(uint8_t terrain);
  /// Take card @p card of the ground folder as the brush, switch to the
  /// Tile tool, and paint one dab of it on @p tile — what dropping a
  /// terrain card on the viewport does.
  void pickGroundCard(size_t card, WorldPoint tile);
  /// One moment of a paint stroke the viewport reported at @p point.
  void paintStroke(EditorStrokePhase phase, WorldPoint point);
  /// Paint the brush's square round @p point into the document, with no
  /// record of it yet: a stroke is recorded once, when it ends.
  void paintBrushAt(WorldPoint point);
  /// Record everything the stroke in flight painted as one edit.
  void endStroke();
  /// Grow or shrink the brush for `[` and `]` while the Tile tool is out.
  /// False for any other key.
  bool handleBrushKey(uint32_t key);
  /// Tell the viewport whether a drag paints, and how wide the brush is.
  void applyBrushToViewport(EditorViewportWidget& viewport) const;
  /// The status line's word on the brush: its terrain and its size.
  [[nodiscard]] std::string brushStatus() const;
  /// Rebuild the ground's mesh if the document's ground has changed since
  /// it was last built.
  void refreshGroundMesh();
  /// Add the ground to the scene, when anything is painted.
  void appendGroundInstance();
  /// Destroy the ground's mesh and its atlas texture.
  void releaseGround();
  /// Set the selected body of water to depth @p index of
  /// `EDITOR_WATER_DEPTHS`, as one undoable edit, as the panel's Depth row
  /// does.
  void deepenSelectedWater(size_t index);
  /// The ground folder's card for what the brush lays down now.
  [[nodiscard]] size_t brushCard() const;
  /// Show the selected body of water in @p panel: its colour and opacity
  /// sliders and its Depth row.
  void showWaterSelection(EditorPropertiesWidget& panel);
  /// Set @p field of every cell of the selected body of water, live while
  /// a slider moves and as one undoable edit when @p edit commits.
  void applyWaterEdit(EditorPropertyField field, float value,
                      EditorPropertyEdit edit);
  /// Record the slider gesture in flight on the water as one edit.
  void commitWaterEdit();
  /// Take the selected body of water off the ground, as one undoable edit,
  /// as the Delete key does.
  void drySelectedWater();
  /// Select the body of water under @p tile, or else the painted area;
  /// false when there is neither.
  bool selectAreaAt(WorldPoint tile);
  /// The cells of the selected area of ground or body of water; none when
  /// neither is selected.
  [[nodiscard]] std::vector<GroundCell> selectedArea() const;
  /// Show the selected area of ground or body of water in @p panel.
  void showAreaSelection(EditorPropertiesWidget& panel);
  /// Erase the selected area of ground or dry the selected body of water,
  /// as the Delete key does; false when neither is selected.
  bool deleteSelectedArea();
  /// Reshape, push and age the water for one frame of @p seconds, and
  /// mirror what it is doing into shell state.
  void tickWater(float seconds);
  /// Where everyone who can wade is standing: the playtest's players and
  /// actors, in the order the playtest lists them; nobody while editing.
  [[nodiscard]] std::vector<Vec2> waderPositions() const;
  /// Rebuild the water's surface if the water has been reshaped since it
  /// was last built, and hand the renderer this frame's ripples.
  void refreshWater();
  /// Draw the water over the scene pass's opaque meshes.
  void drawWater(RhiCommandList& cmd, const EditorViewportWidget& viewport);
  /// Draw water at @p fidelity from now on, and save it as the user's.
  void setWaterFidelity(WaterFidelity fidelity);
  /// Check the View menu's water row, and save the graphics settings, when
  /// they have changed since last time.
  void tickGraphics();
  /// The effects the viewport draws and lights by: the playtest's while
  /// playing, and the editor's own emitters' otherwise.
  [[nodiscard]] const FxWorld& activeEffects() const;
  /// The same, to play into.
  [[nodiscard]] FxWorld& activeEffects();
  /// Copy what the viewport's effects are doing into shell state, for the
  /// agent API.
  void publishEffects();
  /// Move the editor's own effects on by @p seconds, and let every emitter
  /// burst that is due — while editing; a playtest runs its own.
  void tickEditEffects(float seconds);
  /// Move the playtest's effects on by @p seconds, emitters included.
  void advancePlaytestEffects(float seconds);
  /// Stop every effect playing outside a playtest, and let each emitter
  /// burst afresh.
  void resetEditEffects();
  /// Every leg of every patrol route, for the viewport to draw.
  [[nodiscard]] std::vector<EditorRouteLine> routeLines() const;
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
  /// Run @p command when it is one of the Level menu's playtest rows.
  /// Returns true when it was.
  bool runPlaytestCommand(EditorMenuCommand command);
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
  /// Character selector widget id in the tree (owned by the tree).
  GuiWidgetId character_select_id_ = GUI_WIDGET_ID_INVALID;
  /// The Controls screen, over the viewport while it is open.
  GuiWidgetId controls_id_ = GUI_WIDGET_ID_INVALID;
  /// The Sound screen, over the viewport while it is open.
  GuiWidgetId sound_id_ = GUI_WIDGET_ID_INVALID;
  /// Mesh pipeline, uploaded meshes, and the scene depth target.
  MeshRenderer mesh_renderer_{};
  /// Outline pipeline, which reads `mesh_renderer_`'s depth target.
  MeshOutlineRenderer outline_renderer_{};
  /// Skinned-mesh pipeline and the rigged models' uploaded meshes. Draws
  /// in the same pass, against the same depth, as `mesh_renderer_`.
  SkinnedMeshRenderer skinned_renderer_{};
  /// Effects pipeline, which reads `mesh_renderer_`'s depth target after
  /// the outline has drawn.
  FxRenderer fx_renderer_{};
  /// Volumetric-smoke pipeline, which reads the same depth in the same
  /// pass, just before the particles.
  FxVolumeRenderer fx_volume_renderer_{};
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
  /// The selected waypoint as it was when the gesture now in flight began.
  std::optional<EditorWaypoint> waypoint_prior_{};
  /// The selected emitter as it was when the gesture now in flight began.
  std::optional<EditorEmitter> emitter_prior_{};
  /// The selected billboard as it was when its gesture began.
  std::optional<EditorSprite> sprite_prior_{};
  /// The sprite clock when sheets' events were last looked for: where the
  /// next frame's window over it starts.
  double sprite_events_clock_ = 0.0;
  /// The animation events table's revision when last saved.
  uint64_t saved_animation_events_revision_ = 0;
  /// The terrain the brush paints with, numbered as `EDITOR_TERRAINS` is:
  /// 0 erases.
  uint8_t brush_terrain_ = 1;
  /// How many tiles on a side the brush covers.
  int32_t brush_size_ = EDITOR_BRUSH_SIZE_MIN;
  /// The ground as it was when the stroke in flight began, or nothing
  /// between strokes. What the stroke's edit is measured against.
  std::optional<GroundGrid> stroke_before_{};
  /// The water as it was when that stroke began.
  std::optional<WaterLayer> stroke_water_before_{};
  /// What the brush lays down: a terrain, water or dry.
  EditorGroundBrush brush_kind_ = EditorGroundBrush::TERRAIN;
  /// The colour and opacity the brush lays water in where it was dry: the
  /// defaults, or the last body of water selected, so painting more of a
  /// lake paints lake.
  WaterCell brush_water_{};
  /// The water as it was before the slider gesture in flight, or nothing
  /// between gestures. What the gesture's edit is measured against.
  std::optional<WaterLayer> water_prior_{};
  /// Which of `EDITOR_WATER_DEPTHS` the brush lays water at.
  size_t brush_depth_ = EDITOR_DEFAULT_WATER_DEPTH;
  /// The ground the uploaded mesh was built from. Compared with the
  /// document's every frame, so any route that changes the ground — a
  /// stroke, an undo, an agent's fill, a level opened — is drawn without
  /// each of them having to say so.
  GroundGrid drawn_ground_{};
  /// The uploaded ground, or invalid when nothing is painted.
  MeshGpuId ground_mesh_ = MESH_GPU_INVALID;
  /// The terrain swatches the ground is drawn with, made on first use.
  RhiTextureHandle ground_atlas_ = RHI_TEXTURE_INVALID;
  /// The backend's water pipeline, the surface and the ripples' textures.
  WaterRenderer water_renderer_{};
  /// The ripples on the level's water, and what pushes them.
  EditorWater water_{};
  /// The water shape the uploaded surface was built for; 0 before any.
  uint64_t drawn_water_shape_ = 0;
  /// Sheet images uploaded so far, by the path a billboard names them by.
  /// Sorted rather than hashed: nothing here needs a hash, and a sorted
  /// container is one fewer iteration order to have an opinion about.
  std::map<std::string, EditorSpriteSheetTexture> sprite_sheets_{};
  /// Quads uploaded so far, one per frame of a sheet actually shown.
  std::map<EditorSpriteQuadKey, MeshGpuId> sprite_quads_{};
  /// What the level's emitters throw while it is being edited, drawn and
  /// lit by when no playtest is running. Presentation, on the frame clock.
  FxWorld edit_fx_{0};
  /// When each of the level's emitters bursts next.
  EditorEmitterPlayer emitter_player_{};
  /// The level being played, or nothing while editing.
  std::unique_ptr<EditorPlaytestSession> playtest_{};
  /// The build running in the background, or the last one to.
  EditorBuildJob build_job_{};
  /// The project's game logic as last built and loaded; the next playtest
  /// runs it. Null with none.
  std::shared_ptr<EditorLogicLibrary> logic_library_{};
  /// Loads of a logic library this session, which names each load's copy.
  uint32_t logic_loads_ = 0;
  /// The project the build state describes; empty with none.
  std::filesystem::path build_root_{};
  /// The project the running — or last — build was started for. A build
  /// that ends after that project is closed has nothing left to load into.
  std::filesystem::path building_root_{};
  /// The clip each combat cue plays, loaded into the client's audio bank
  /// when the editor starts.
  game::CombatSoundClips combat_sounds_{};
  /// The clip each step set plays on each surface, in the audio bank.
  game::FootstepSoundClips footstep_sounds_{};
  /// When the last frame of play ran, for the playtest's clock.
  std::chrono::steady_clock::time_point playtest_frame_{};
  /// How far the last frame of play got between its two newest ticks,
  /// which is where the players are drawn.
  float playtest_alpha_ = 0.0f;
  /// The actions the keys held right now ask for.
  input::HeldActions held_actions_{};
  /// The controls revision last written to their file.
  uint64_t saved_controls_revision_ = 0;
  /// The volume revision last applied and written; none until the first
  /// frame applies them.
  std::optional<uint64_t> saved_sound_revision_{};
  /// The graphics revision last applied and written; none until the first
  /// frame applies it.
  std::optional<uint64_t> saved_graphics_revision_{};
  /// The sounds table revision last written and loaded.
  uint64_t saved_sounds_revision_ = 0;
  /// The slot an import picked from the Sound screen goes into, if any.
  std::string import_sound_slot_{};
  /// Which pad plays as which player: seat 0 is player 1.
  input::GamepadSeats seats_{};
};

}  // namespace eng::editor
