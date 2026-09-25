// The playtest half of SimplishEditor: starting and stopping one, feeding it
// input, and drawing what it simulates. Kept apart from simplish-editor.cpp
// because it is the one part of the editor that runs the game rather than
// authoring it, and everything here is gated on a playtest existing.

#include <algorithm>
#include <chrono>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-character-card.h>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-character-transform.h>
#include <editor/shell/editor-footstep-surfaces.h>
#include <editor/shell/editor-placement-clip.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-playtest-controls.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-projection.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/core/logger.h>
#include <engine/input/action-values.h>
#include <engine/input/gamepad-actions.h>
#include <engine/input/input-action.h>
#include <engine/input/player-input-builder.h>
#include <engine/math/mat4.h>
#include <game/combat/combat-system.h>
#include <string>

namespace eng::editor {

namespace {

  using Keycode = eng::client::DesktopPlatformKeycode;

  /// How wide and how tall the stand-in for a player is drawn, in tiles:
  /// about a person, and narrower than a tile so two can stand side by side.
  constexpr float STAND_IN_WIDTH = 0.6f;
  constexpr float STAND_IN_HEIGHT = 1.5f;

  /// Scale by the stand-in's proportions about @p feet, which stay put.
  Mat4 standInScale(Vec3 feet) {
    Mat4 out = Mat4::identity();
    out(0, 0) = STAND_IN_WIDTH;
    out(1, 1) = STAND_IN_WIDTH;
    out(2, 2) = STAND_IN_HEIGHT;
    out(0, 3) = feet.x * (1.0f - STAND_IN_WIDTH);
    out(1, 3) = feet.y * (1.0f - STAND_IN_WIDTH);
    out(2, 3) = feet.z * (1.0f - STAND_IN_HEIGHT);
    return out;
  }

  /// The transform that stands @p shape — a built-in shape one tile across
  /// — on @p feet, at a player's proportions.
  Mat4 standInTransform(const EditorAsset& shape, Vec3 feet) {
    EditorPlacement placed;
    placed.position = {feet.x - 0.5f, feet.y - 0.5f, feet.z};
    return standInScale(feet) * makePlacementTransform(shape, placed);
  }

}  // namespace

bool SimplishEditor::isPlaying() const {
  return playtest_ != nullptr;
}

void SimplishEditor::togglePlaytest() {
  if (isPlaying() || state_.playtest.mode == EditorPlayMode::CHOOSING) {
    stopPlaytest();
  } else {
    requestPlaytest();
  }
}

void SimplishEditor::requestPlaytest() {
  if (!state_.project.loaded) {
    showStatusMessage("Open a project to play a level");
    return;
  }
  // Read again on every Play, so a hand edit to a table reaches this
  // playtest without reopening the project.
  reloadDataTables();
  const auto& characters = state_.characters.characters;
  if (characters.size() >= 2) {
    openCharacterSelect();
    return;
  }
  startPlaytestAs(characters.empty() ? std::string{} : characters.front().id);
}

void SimplishEditor::reloadCharacters() {
  state_.characters = state_.project.loaded
                          ? loadEditorCharacterTable(state_.project.root)
                          : EditorCharacterTable{};
  for (const std::string& problem : state_.characters.problems) {
    LOG_WARN("editor", "characters.data.json: " + problem);
  }
}

void SimplishEditor::reloadBehaviors() {
  state_.behaviors = state_.project.loaded
                         ? loadEditorBehaviorTable(state_.project.root)
                         : EditorBehaviorTable{};
  for (const std::string& problem : state_.behaviors.problems) {
    LOG_WARN("editor", "behaviors.data.json: " + problem);
  }
}

void SimplishEditor::reloadEnemies() {
  state_.enemies = state_.project.loaded
                       ? loadEditorEnemyTable(state_.project.root)
                       : EditorEnemyTable{};
  for (const std::string& problem : state_.enemies.problems) {
    LOG_WARN("editor", "enemies.data.json: " + problem);
  }
}

void SimplishEditor::reloadUi() {
  state_.ui = state_.project.loaded ? loadEditorUiTable(state_.project.root)
                                    : EditorUiTable{};
  for (const std::string& problem : state_.ui.problems) {
    LOG_WARN("editor", "content/ui/" + problem);
  }
  // A screen shown may have changed: build them again on the next sync.
  game_ui_shown_.clear();
}

void SimplishEditor::reloadDataTables() {
  reloadCharacters();
  reloadBehaviors();
  reloadEnemies();
  reloadUi();
  reloadSounds();
  reloadAnimationEvents();
}

EditorCharacterSelectWidget* SimplishEditor::characterSelectWidget() {
  return dynamic_cast<EditorCharacterSelectWidget*>(
      guiWidgetTree().findWidget(character_select_id_));
}

void SimplishEditor::openCharacterSelect() {
  EditorCharacterSelectWidget* selector = characterSelectWidget();
  if (selector == nullptr) {
    return;
  }
  commitPendingEdit();
  select({});
  const auto& characters = state_.characters.characters;
  const std::string preferred =
      editorPlaytestDefaultCharacter(state_.document, characters);
  const auto found =
      std::ranges::find(characters, preferred, &game::CharacterDefinition::id);
  selector->open(makeEditorCharacterCards(characters, state_.assets),
                 static_cast<size_t>(found - characters.begin()));
  state_.playtest.mode = EditorPlayMode::CHOOSING;
  applyPlayModeToChrome();
  showStatusMessage("Choose a character to play " + state_.level_id);
}

void SimplishEditor::closeCharacterSelect() {
  if (EditorCharacterSelectWidget* selector = characterSelectWidget()) {
    selector->close();
  }
  if (state_.playtest.mode == EditorPlayMode::CHOOSING) {
    state_.playtest.mode = EditorPlayMode::EDITING;
    applyPlayModeToChrome();
  }
}

bool SimplishEditor::handleChoosingKey(uint32_t key) {
  EditorCharacterSelectWidget* selector = characterSelectWidget();
  if (selector == nullptr) {
    return false;
  }
  if (key == Keycode::ARROW_LEFT || key == Keycode::ARROW_UP) {
    selector->moveHighlight(-1);
  } else if (key == Keycode::ARROW_RIGHT || key == Keycode::ARROW_DOWN) {
    selector->moveHighlight(1);
  } else if (key == Keycode::KEY_RETURN) {
    selector->confirm();
  } else if (key == Keycode::ESCAPE) {
    selector->cancel();
  } else {
    return false;
  }
  return true;
}

WorldPoint SimplishEditor::playtestFallback() {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return {0.5f, 0.5f, 0.0f};
  }
  // The middle of the tile under the middle of the viewport: where the
  // camera is looking, which is Editor §7's "camera position".
  const Rect& rect = viewport->rect;
  const WorldPoint at =
      screenToWorld(makeIsoView(viewport->camera, rect),
                    {rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f});
  return {std::floor(at.x) + 0.5f, std::floor(at.y) + 0.5f, 0.0f};
}

void SimplishEditor::startPlaytestAs(const std::string& character) {
  if (!state_.project.loaded || isPlaying()) {
    return;
  }
  closeCharacterSelect();
  // The gesture in flight is part of the level being played; the selection
  // is not, and its panel would be a way to edit mid-game.
  commitPendingEdit();
  select({});
  const EditorPlaytestRun run = playtestRun();
  playtest_ = std::make_unique<EditorPlaytestSession>(
      playtestSetup(character, run), playtestContent(), run);
  beginPlaytestState();
  (void)avatarAsset();
  applyPlayModeToChrome();
  showStatusMessage(playingMessage());
}

game::GameSetup SimplishEditor::playtestSetup(const std::string& character,
                                              const EditorPlaytestRun& run) {
  game::GameSetup setup = makeEditorPlaytestSetup(
      state_.document, state_.assets, playtestFallback());
  setup.characters[0] = character;
  addPlaytestPlayers(setup);
  if (run.logic != nullptr) {
    // Room for the logic to spawn into.
    setup.actor_capacity = game::GAME_LOGIC_ACTOR_CAPACITY;
  }
  return setup;
}

void SimplishEditor::addPlaytestPlayers(game::GameSetup& setup) const {
  // Room for everyone holding a pad, and stand-ins in whatever seats are
  // left up to the number asked for.
  addEditorStandIns(
      setup, state_.document,
      std::max(state_.playtest_stand_ins, editorPadPlayers(seats_)));
}

std::string SimplishEditor::playingMessage() const {
  const std::string& name = playtest_->character(0).name;
  const uint8_t pads = editorPadPlayers(seats_);
  const std::string on_pads =
      pads == 0 ? "" : ", players 2–" + std::to_string(pads + 1) + " on pads";
  return "Playing " + state_.level_id + (name.empty() ? "" : " as " + name) +
         on_pads + " — F5 or Esc to stop" + playtestLogicNote();
}

game::GameContent SimplishEditor::playtestContent() const {
  game::GameContent content{state_.characters.characters,
                            state_.behaviors.behaviors, state_.enemies.enemies};
  addEditorUiContent(state_.ui, content);
  return content;
}

void SimplishEditor::beginPlaytestState() {
  playtest_->setActorIds(editorActorIds(state_.document));
  // Events are heard from the first frame of play on: nothing played
  // before it — while no scene was drawn, perhaps — reaches into it.
  sprite_events_clock_ = animation_clock_;
  (void)placement_animator_.takePasses();
  playtest_->setActorFootsteps(editorActorFootsteps(state_.document));
  playtest_->setFootstepSurfaces(
      makeEditorFootstepSurfaces(state_.document, state_.assets));
  state_.playtest = EditorPlaytestState{};
  state_.playtest.mode = EditorPlayMode::PLAYING;
  playtest_->publish(state_.playtest);
  playtest_frame_ = std::chrono::steady_clock::now();
  playtest_alpha_ = 0.0f;
  held_actions_.releaseAll();
  // The playtest's effects start empty; its emitters burst into them at
  // once, as they did when they were dropped.
  resetEditEffects();
}

void SimplishEditor::stopPlaytest() {
  closeCharacterSelect();
  if (!isPlaying()) {
    return;
  }
  saveLastPlaytestReplay();
  audio().stopAll();
  clearGameUi();
  playtest_.reset();
  resetEditEffects();
  state_.playtest = EditorPlaytestState{};
  held_actions_.releaseAll();
  applyPlayModeToChrome();
  refreshPlacementMarkers();
  showStatusMessage("Stopped playing; the level is as it was");
}

void SimplishEditor::saveLastPlaytestReplay() {
  if (writeEditorPlaytestReplay(state_.project.root, state_.level_id,
                                playtest_->replay())) {
    return;
  }
  LOG_WARN("editor",
           "Could not write the playtest replay to " +
               editorPlaytestReplayPath(state_.project.root, state_.level_id)
                   .string());
}

void SimplishEditor::tickPlaytest() {
  if (!isPlaying()) {
    return;
  }
  // Paused, the frame's time is spent rather than owed, so resuming does
  // not arrive with a burst of ticks the pause saved up.
  const uint64_t elapsed = playtestElapsedNs();
  if (state_.playtest.clock == EditorPlaytestClock::PAUSED) {
    return;
  }
  feedPadPlayers();
  takeAgentUiPress();
  const FixedStepAdvance due =
      playtest_->advance(elapsed, livePlayerInput(), state_.playtest.scripted);
  playtest_alpha_ = due.interpolation;
  // Effects run on the frame's own time, as clips do, and stop with a
  // pause so a paused frame can be looked at.
  advancePlaytestEffects(static_cast<float>(elapsed) * 1e-9f);
  afterPlaytestTicks();
}

void SimplishEditor::afterPlaytestTicks() {
  // What the ticks did to player 1, felt through the pad in their seat.
  const input::GamepadRumble felt = playtest_->takeRumble();
  if (const std::optional<uint64_t> pad = seats_.device(0);
      pad && input::isRumbling(felt)) {
    (void)rumbleGamepad(*pad, felt);
  }
  hearPlaytest();
  playtest_->publish(state_.playtest);
  syncGameUi();
  followPlayer();
  refreshPlacementMarkers();
}

void SimplishEditor::togglePlaytestPause() {
  if (!isPlaying()) {
    return;
  }
  const bool paused = state_.playtest.clock == EditorPlaytestClock::PAUSED;
  state_.playtest.clock =
      paused ? EditorPlaytestClock::RUNNING : EditorPlaytestClock::PAUSED;
  applyPlayModeToChrome();
  showStatusMessage(paused ? "Resumed"
                           : "Paused — F7 steps one tick, F6 resumes");
}

void SimplishEditor::stepPlaytest(uint32_t ticks) {
  if (!isPlaying()) {
    return;
  }
  state_.playtest.clock = EditorPlaytestClock::PAUSED;
  feedPadPlayers();
  takeAgentUiPress();
  for (uint32_t i = 0; i < ticks; ++i) {
    playtest_->step(livePlayerInput(), state_.playtest.scripted);
    // A step is a tick's worth of time for the effects too.
    advancePlaytestEffects(1.0f / static_cast<float>(TICK_RATE_HZ));
  }
  // Drawn where the last tick left everything, not partway to a next tick
  // that is not coming.
  playtest_alpha_ = 1.0f;
  afterPlaytestTicks();
  applyPlayModeToChrome();
}

uint64_t SimplishEditor::playtestElapsedNs() {
  // The editor is not the simulation and may read the clock; the tick
  // count that comes out of this is all the simulation ever sees.
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = now - playtest_frame_;
  playtest_frame_ = now;
  const auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
  return ns > 0 ? static_cast<uint64_t>(ns) : 0U;
}

sim::PlayerInput SimplishEditor::livePlayerInput() {
  // Keys and the pad in use through the same bindings, the stronger of the
  // two winning, so either can be picked up mid-run.
  input::ActionValues values{held_actions_};
  if (const input::GamepadState* pad = playerOnePad()) {
    input::offerGamepad(values, *pad, state_.controls.bindings);
  }
  const EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return input::makePlayerInput(values, {}, input::MoveBasis{});
  }
  if (viewport->leftHeld()) {
    values.offer(input::InputAction::FIRE, 1.0f);
  }
  // Movement and stick aim follow the camera: up is up the screen, whichever
  // of the two projections the project draws with. A resting aim stick
  // leaves the cursor aiming — unless the pad is what is in use, when a
  // cursor left lying in the viewport would drag the aim to it; then the
  // player keeps the aim they had.
  const Vec2 fallback =
      inputMethod() == input::InputMethod::GAMEPAD ? Vec2{} : cursorAim();
  return input::makePlayerInput(values, fallback,
                                editorMoveBasis(viewport->camera.axes));
}

const input::GamepadState* SimplishEditor::playerOnePad() const {
  const std::optional<uint64_t> device = seats_.device(0);
  return device ? gamepads().state(*device) : nullptr;
}

void SimplishEditor::feedPadPlayers() {
  const EditorViewportWidget* viewport = viewportWidget();
  const input::MoveBasis basis = viewport != nullptr
                                     ? editorMoveBasis(viewport->camera.axes)
                                     : input::MoveBasis{};
  for (uint8_t slot = 1; slot < sim::MAX_PLAYERS; ++slot) {
    const std::optional<uint64_t> device = seats_.device(slot);
    const input::GamepadState* pad =
        device ? gamepads().state(*device) : nullptr;
    playtest_->setPadInput(
        slot, pad != nullptr ? std::optional{editorPadInput(
                                   *pad, state_.controls.bindings, basis)}
                             : std::nullopt);
  }
}

Vec2 SimplishEditor::cursorAim() {
  const EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr || !viewport->hasHover() ||
      playtest_->players().slots.size() == 0) {
    return {};
  }
  const WorldPoint target =
      screenToWorld(makeIsoView(viewport->camera, viewport->rect),
                    viewport->hoveredScreenPoint());
  const Vec3 at = playtest_->players().position[0];
  return {target.x - at.x, target.y - at.y};
}

void SimplishEditor::followPlayer() {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr || playtest_->players().slots.size() == 0) {
    return;
  }
  // Every frame, which is also what keeps a drag from panning away from
  // the player: the pan lands and is overwritten before anything draws.
  const Vec3 at = playtest_->renderPosition(0, playtest_alpha_);
  viewport->camera.focus =
      worldToIso(viewport->camera.axes, {at.x, at.y, at.z});
}

void SimplishEditor::hearPlaytest() {
  audio().setListener(playtestListener());
  for (const game::CombatCue& cue : playtest_->takeHeardCues()) {
    (void)audio().play(game::combatCueSound(cue, combat_sounds_));
  }
  for (const game::FootstepCue& step : playtest_->takeHeardSteps()) {
    (void)audio().play(game::footstepSound(step, footstep_sounds_));
  }
  for (const game::WorldCue& cue : playtest_->takeLogicCues()) {
    playLogicCue(cue);
  }
}

audio::AudioListener SimplishEditor::playtestListener() {
  audio::AudioListener listener;
  if (playtest_->players().slots.size() > 0) {
    listener.at = playtest_->renderPosition(0, playtest_alpha_);
  }
  if (const EditorViewportWidget* viewport = viewportWidget()) {
    listener.right = editorMoveBasis(viewport->camera.axes).right;
  }
  return listener;
}

void SimplishEditor::appendPlaytestMarkers(
    std::vector<EditorPlacementMarker>& markers) {
  if (!isPlaying()) {
    return;
  }
  const game::PlayerPool& pool = playtest_->players();
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    const Vec3 at = playtest_->renderPosition(i, playtest_alpha_);
    const auto player = static_cast<uint8_t>(pool.input_slot[i] + 1U);
    const EditorPlayerStart standing{{}, player, {at.x, at.y, at.z}};
    markers.push_back({editorPlayerStartBounds(standing), false,
                       EditorMarkerStyle::PLAYER_START, player});
  }
  appendCombatMarkers(markers);
}

void SimplishEditor::appendCombatMarkers(
    std::vector<EditorPlacementMarker>& markers) const {
  constexpr float SHOT = game::PROJECTILE_RADIUS_TILES;
  for (const WorldPoint& at : state_.playtest.projectiles) {
    markers.push_back({{{at.x - SHOT, at.y - SHOT, at.z - SHOT},
                        {at.x + SHOT, at.y + SHOT, at.z + SHOT}},
                       false,
                       EditorMarkerStyle::PROJECTILE});
  }
  for (const EditorPlaytestHazard& pool : state_.playtest.hazards) {
    const WorldPoint& at = pool.position;
    markers.push_back({{{at.x - pool.radius, at.y - pool.radius, at.z},
                        {at.x + pool.radius, at.y + pool.radius, at.z}},
                       false,
                       EditorMarkerStyle::HAZARD});
  }
}

std::vector<EditorCharacterFigure> SimplishEditor::characterFigures() const {
  if (!isPlaying()) {
    return editorStartFigures(state_.document, state_.characters.characters);
  }
  std::vector<EditorCharacterFigure> figures;
  const game::PlayerPool& pool = playtest_->players();
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    const auto player = static_cast<uint8_t>(pool.input_slot[i] + 1U);
    figures.push_back({editorPlayerFigureKey(player),
                       playtest_->character(i).model,
                       playtest_->renderPosition(i, playtest_alpha_),
                       pool.aim[i], playtest_->gait(i)});
  }
  appendSpawnedFigures(figures);
  return figures;
}

void SimplishEditor::appendSpawnedFigures(
    std::vector<EditorCharacterFigure>& figures) const {
  // No prop stands for an actor the game logic spawned, so it is drawn
  // the way a player is: its model, or the stand-in.
  const game::ActorPool& pool = playtest_->actors();
  for (const uint32_t i : playtest_->spawnedActors()) {
    const sim::EntityHandle handle = pool.slots.handleAt(i);
    figures.push_back({"spawned:" + std::to_string(handle.index) + ":" +
                           std::to_string(handle.generation),
                       std::string(playtest_->actorModel(i)),
                       playtest_->actorRenderPosition(i, playtest_alpha_),
                       pool.facing[i], playtest_->actorGait(i)});
  }
}

void SimplishEditor::appendCharacterInstances() {
  for (const EditorCharacterFigure& figure : characterFigures()) {
    appendCharacterInstance(figure);
  }
}

void SimplishEditor::appendCharacterInstance(
    const EditorCharacterFigure& figure) {
  if (const std::optional<size_t> index = characterAsset(figure.model)) {
    const EditorAsset& asset = state_.assets[*index];
    if (asset.rig != nullptr && asset.skinned_mesh != MESH_GPU_INVALID) {
      appendSkinnedCharacter(figure, *index);
      return;
    }
    scene_instances_.push_back(
        {asset.mesh,
         makeEditorCharacterTransform(asset, figure.feet, figure.aim),
         asset.texture});
    return;
  }
  appendStandIn(figure.feet);
}

void SimplishEditor::appendStandIn(Vec3 feet) {
  if (const std::optional<size_t> shape = avatarAsset()) {
    const EditorAsset& asset = state_.assets[*shape];
    scene_instances_.push_back(
        {asset.mesh, standInTransform(asset, feet), asset.texture});
  }
}

void SimplishEditor::appendSkinnedCharacter(const EditorCharacterFigure& figure,
                                            size_t asset) {
  const EditorAsset& model = state_.assets[asset];
  // Posed through the props' animator, under a key no prop has, so a
  // player who stops fades from walking to standing as a prop fades
  // between the clips the panel picks.
  EditorPlacement posed;
  posed.id = figure.key;
  posed.asset = asset;
  // Where its feet are, so the clip's events are heard from there.
  posed.position = {figure.feet.x, figure.feet.y, figure.feet.z};
  posed.animation =
      editorCharacterClip(editorClipNames(model.rig.get()), figure.gait);
  skinned_instances_.push_back(
      {model.skinned_mesh,
       makeEditorCharacterTransform(model, figure.feet, figure.aim),
       model.texture,
       placement_animator_.pose(posed, *model.rig, animation_clock_)});
}

void SimplishEditor::refreshActorOverlays() {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  viewport->actor_overlays.clear();
  if (!viewport->show_ai || !isPlaying()) {
    return;
  }
  // Every actor in the game, placed by the level or spawned by its logic.
  for (uint32_t i = 0; i < playtest_->actors().slots.size(); ++i) {
    viewport->actor_overlays.push_back(
        playtest_->actorOverlay(i, playtest_alpha_));
  }
}

EditorPlacement SimplishEditor::posedActor(size_t index, size_t actor) const {
  const EditorPlacement& placement = state_.document.placements[index];
  const std::optional<uint32_t> dense =
      isPlaying() ? playtest_->actorIndex(actor) : std::nullopt;
  if (!dense) {
    return placement;
  }
  return editorActorPose(
      placement, playtest_->actorRenderPosition(*dense, playtest_alpha_),
      playtest_->actors().facing[*dense]);
}

std::string SimplishEditor::actorClip(const EditorPlacement& placement,
                                      size_t actor) const {
  const std::optional<uint32_t> dense = playtest_->actorIndex(actor);
  if (!dense || placement.asset >= state_.assets.size()) {
    return placement.animation;
  }
  const std::vector<std::string> clips =
      editorClipNames(state_.assets[placement.asset].rig.get());
  const std::string& wanted = playtest_->actorState(*dense).clip;
  if (!wanted.empty() && std::ranges::find(clips, wanted) != clips.end()) {
    return wanted;
  }
  return editorCharacterClip(clips, playtest_->actorGait(*dense));
}

void SimplishEditor::appendActorInstance(size_t index, size_t actor) {
  EditorPlacement posed = posedActor(index, actor);
  if (isPlaying()) {
    posed.animation = actorClip(posed, actor);
  }
  appendPlacementInstance(posed);
}

std::optional<size_t> SimplishEditor::characterAsset(const std::string& model) {
  const std::optional<size_t> index =
      findEditorAssetByRef(state_.assets, model);
  return index && ensureAssetMesh(*index) ? index : std::nullopt;
}

std::optional<size_t> SimplishEditor::avatarAsset() {
  for (size_t i = 0; i < state_.assets.size(); ++i) {
    if (state_.assets[i].shape == EditorShapeKind::CYLINDER) {
      return ensureAssetMesh(i) ? std::optional{i} : std::nullopt;
    }
  }
  return std::nullopt;
}

bool SimplishEditor::handlePlaytestKey(uint32_t key, ClientKeyDownKind kind) {
  if (key == Keycode::F5) {
    if (kind == ClientKeyDownKind::FIRST_PRESS) {
      togglePlaytest();
    }
    return true;
  }
  if (handleClockKey(key, kind) ||
      (kind == ClientKeyDownKind::FIRST_PRESS && handleGameUiKey(key))) {
    return true;
  }
  if (state_.playtest.mode == EditorPlayMode::CHOOSING) {
    // Every key, taken or not: the selector is modal, and a stray Delete
    // must not reach the level behind it.
    (void)handleChoosingKey(key);
    return true;
  }
  return isPlaying() && handlePlayingKey(key);
}

bool SimplishEditor::handleClockKey(uint32_t key, ClientKeyDownKind kind) {
  if (!isPlaying() || (key != Keycode::F6 && key != Keycode::F7)) {
    return false;
  }
  // F7 repeats while held, which is how a run of ticks is walked through;
  // F6 toggles, so it acts on the press alone.
  if (key == Keycode::F7) {
    stepPlaytest(1);
  } else if (kind == ClientKeyDownKind::FIRST_PRESS) {
    togglePlaytestPause();
  }
  return true;
}

bool SimplishEditor::handlePlayingKey(uint32_t key) {
  if (key == Keycode::ESCAPE) {
    stopPlaytest();
    return true;
  }
  const std::vector<input::InputAction> actions =
      state_.controls.bindings.actionsFor(input::InputSource::key(key));
  for (const input::InputAction action : actions) {
    held_actions_.press(action);
  }
  return !actions.empty();
}

void SimplishEditor::onClientKeyUp(uint32_t key) {
  for (const input::InputAction action :
       state_.controls.bindings.actionsFor(input::InputSource::key(key))) {
    held_actions_.release(action);
  }
}

void SimplishEditor::onClientGamepadButtonDown(input::GamepadButton button) {
  if (handleControlsButton(button) || handleSoundButton(button) ||
      handleGameUiButton(button)) {
    return;
  }
  if (state_.playtest.mode == EditorPlayMode::CHOOSING) {
    if (const std::optional<uint32_t> key =
            editorChoosingKeyFor(button, gamepads().activeFamily())) {
      (void)handleChoosingKey(*key);
    }
    return;
  }
  // Start is a pad's pause button on every platform; F6 is the keyboard's.
  if (isPlaying() && button == input::GamepadButton::START) {
    togglePlaytestPause();
  }
}

void SimplishEditor::onClientFocusLost() {
  // The releases of whatever was held will never arrive, and a player who
  // keeps walking after the window is left is a bug nobody can stop.
  held_actions_.releaseAll();
}

void SimplishEditor::applyPlayModeToChrome() {
  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->setPlayMode(state_.playtest.mode);
  }
  if (auto* menu =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    menu->setPlayMode(state_.playtest.mode);
    menu->setPlaytestClock(state_.playtest.clock);
    menu->setStandIns(state_.playtest_stand_ins);
  }
}

std::string SimplishEditor::playtestStatus() const {
  std::string status = (state_.playtest.clock == EditorPlaytestClock::PAUSED
                            ? "paused    tick "
                            : "playing   tick ") +
                       std::to_string(state_.playtest.tick);
  if (state_.playtest.dropped_ticks > 0) {
    status += "   dropped " + std::to_string(state_.playtest.dropped_ticks);
  }
  return status + "   " + playerOneHealth();
}

std::string SimplishEditor::playerOneHealth() const {
  if (state_.playtest.run_over) {
    return "run over — F5 or Esc to stop";
  }
  for (const EditorPlaytestPlayer& player : state_.playtest.players) {
    if (player.player != 1) {
      continue;
    }
    if (player.downed) {
      return "down — a teammate can revive you";
    }
    return "health " + std::to_string(player.health) + "/" +
           std::to_string(player.max_health);
  }
  return {};
}

}  // namespace eng::editor
