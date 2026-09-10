// The playtest half of SimplishEditor: starting and stopping one, feeding it
// input, and drawing what it simulates. Kept apart from simplish-editor.cpp
// because it is the one part of the editor that runs the game rather than
// authoring it, and everything here is gated on a playtest existing.

#include <chrono>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-playtest-controls.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-projection.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/core/logger.h>
#include <engine/input/input-action.h>
#include <engine/input/player-input-builder.h>
#include <engine/math/mat4.h>
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
  if (isPlaying()) {
    stopPlaytest();
  } else {
    startPlaytest();
  }
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

void SimplishEditor::startPlaytest() {
  if (!state_.project.loaded) {
    showStatusMessage("Open a project to play a level");
    return;
  }
  // The gesture in flight is part of the level being played; the selection
  // is not, and its panel would be a way to edit mid-game.
  commitPendingEdit();
  select({});
  playtest_ = std::make_unique<EditorPlaytestSession>(
      makeEditorPlaytestSetup(state_.document, state_.assets,
                              playtestFallback()),
      state_.level_id);
  beginPlaytestState();
  (void)avatarAsset();
  applyPlayModeToChrome();
  showStatusMessage("Playing " + state_.level_id + " — F5 or Esc to stop");
}

void SimplishEditor::beginPlaytestState() {
  state_.playtest = EditorPlaytestState{};
  state_.playtest.mode = EditorPlayMode::PLAYING;
  playtest_->publish(state_.playtest);
  playtest_frame_ = std::chrono::steady_clock::now();
  playtest_alpha_ = 0.0f;
  held_actions_.releaseAll();
}

void SimplishEditor::stopPlaytest() {
  if (!isPlaying()) {
    return;
  }
  saveLastPlaytestReplay();
  playtest_.reset();
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
  const FixedStepAdvance due = playtest_->advance(
      playtestElapsedNs(), livePlayerInput(), state_.playtest.scripted);
  playtest_alpha_ = due.interpolation;
  playtest_->publish(state_.playtest);
  followPlayer();
  refreshPlacementMarkers();
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
  input::HeldActions held = held_actions_;
  const EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return input::makePlayerInput(held, {}, input::MoveBasis{});
  }
  if (viewport->leftHeld()) {
    held.press(input::InputAction::FIRE);
  }
  // Movement follows the camera: up is up the screen, whichever of the two
  // projections the project draws with.
  return input::makePlayerInput(held, cursorAim(),
                                editorMoveBasis(viewport->camera.axes));
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
}

void SimplishEditor::appendPlaytestInstances() {
  if (!isPlaying()) {
    return;
  }
  const std::optional<size_t> shape = avatarAsset();
  if (!shape) {
    return;
  }
  const EditorAsset& asset = state_.assets[*shape];
  for (uint32_t i = 0; i < playtest_->players().slots.size(); ++i) {
    const Vec3 at = playtest_->renderPosition(i, playtest_alpha_);
    scene_instances_.push_back(
        {asset.mesh, standInTransform(asset, at), asset.texture});
  }
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
  return isPlaying() && handlePlayingKey(key);
}

bool SimplishEditor::handlePlayingKey(uint32_t key) {
  if (key == Keycode::ESCAPE) {
    stopPlaytest();
    return true;
  }
  const std::optional<input::InputAction> action = editorPlaytestAction(key);
  if (action) {
    held_actions_.press(*action);
  }
  return action.has_value();
}

void SimplishEditor::onClientKeyUp(uint32_t key) {
  if (const std::optional<input::InputAction> action =
          editorPlaytestAction(key)) {
    held_actions_.release(*action);
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
  }
}

std::string SimplishEditor::playtestStatus() const {
  std::string status = "playing   tick " + std::to_string(state_.playtest.tick);
  if (state_.playtest.dropped_ticks > 0) {
    status += "   dropped " + std::to_string(state_.playtest.dropped_ticks);
  }
  return status;
}

}  // namespace eng::editor
