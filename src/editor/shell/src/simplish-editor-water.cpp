// SimplishEditor's water: the user's fidelity, from the View menu or an
// agent, saved with their graphics settings; the ripples, reshaped when the
// ground or the fidelity changes and pushed by whoever wades through them
// and whatever lands in them; and the surface drawn over the ground's own
// flat water.

#include <cmath>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-graphics-file.h>
#include <editor/shell/editor-menu-bar-widget.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-shell-selection.h>
#include <editor/shell/editor-water-depth-choices.h>
#include <editor/shell/editor-water-ops.h>
#include <editor/shell/simplish-editor.h>
#include <engine/render-fx/fx-world.h>
#include <engine/render-water/water-splash.h>
#include <engine/render-water/water-surface-mesh.h>
#include <string>
#include <utility>

namespace eng::editor {

void SimplishEditor::setGraphicsSettingsPath(
    const std::filesystem::path& path) {
  state_.graphics = loadEditorGraphics(path);
  saved_graphics_revision_.reset();
}

void SimplishEditor::setWaterFidelity(WaterFidelity fidelity) {
  state_.graphics.water = fidelity;
  ++state_.graphics.revision;
  showStatusMessage("Water: " + std::string(waterFidelityWord(fidelity)));
}

void SimplishEditor::setInterfaceScale(float scale) {
  state_.graphics.ui_scale = scale;
  ++state_.graphics.revision;
  showStatusMessage(
      "Interface: " + std::to_string(static_cast<int>(scale * 100.0f + 0.5f)) +
      "%");
}

void SimplishEditor::toggleWaterEffect(WaterEffect effect) {
  bool& on = state_.graphics.water_effects.on[waterEffectIndex(effect)];
  on = !on;
  ++state_.graphics.revision;
  showStatusMessage("Water " + std::string(waterEffectWord(effect)) +
                    (on ? ": on" : ": off"));
}

void SimplishEditor::tickGraphics() {
  if (saved_graphics_revision_ == state_.graphics.revision) {
    return;
  }
  if (auto* menu = dynamic_cast<EditorMenuBarWidget*>(
          guiWidgetTree().findWidget(menu_bar_id_))) {
    menu->setWaterFidelity(state_.graphics.water);
    menu->setWaterEffects(state_.graphics.water_effects);
    menu->setUiScale(state_.graphics.ui_scale);
  }
  // A new scale changes the layout's size, which layoutChrome notices.
  setUiScale(state_.graphics.ui_scale);
  // The first frame only applies what was read; there is nothing new to
  // write back.
  if (saved_graphics_revision_) {
    (void)saveEditorGraphics(state_.graphics);
  }
  saved_graphics_revision_ = state_.graphics.revision;
}

std::vector<Vec2> SimplishEditor::waderPositions() const {
  std::vector<Vec2> waders;
  waders.reserve(state_.playtest.players.size() +
                 state_.playtest.actors.size());
  for (const EditorPlaytestPlayer& player : state_.playtest.players) {
    waders.push_back({player.position.x, player.position.y});
  }
  for (const EditorPlaytestActor& actor : state_.playtest.actors) {
    waders.push_back({actor.position.x, actor.position.y});
  }
  return waders;
}

std::vector<WaterObstacle> SimplishEditor::waterObstacles() const {
  static const EditorAsset missing{};
  std::vector<WaterObstacle> obstacles;
  for (const EditorPlacement& placement : state_.document.placements) {
    const EditorAsset& asset = placement.asset < state_.assets.size()
                                   ? state_.assets[placement.asset]
                                   : missing;
    const PlacementBounds box = placementWorldBounds(asset, placement);
    if (!isEditorActor(placement) && box.min.z < WATER_SURFACE_HEIGHT &&
        box.max.z > WATER_SURFACE_HEIGHT) {
      obstacles.push_back({{box.min.x, box.min.y}, {box.max.x, box.max.y}});
    }
  }
  return obstacles;
}

void SimplishEditor::tickWater(float seconds) {
  water_.reshape(state_.document.water, state_.graphics.water,
                 waterObstacles());
  if (isPlaying()) {
    water_.splash(playtest_->takeCues());
    water_.wade(waderPositions());
  } else {
    water_.forgetWaders();
  }
  // Paused, the water holds still with everything else, so a paused frame
  // can be looked at.
  if (!presentationFrozen()) {
    water_.advance(seconds);
  }
  throwSplashes();
  water_.publish(state_.water);
}

void SimplishEditor::throwSplashes() {
  for (const EditorWaterSplash& splash : water_.takeSplashes()) {
    playFxEffect(activeEffects(), waterSplashEffect(),
                 {{splash.at.x, splash.at.y, WATER_SURFACE_HEIGHT},
                  {0.0f, 0.0f, 1.0f},
                  splash.scale});
  }
}

void SimplishEditor::refreshWater() {
  RhiDevice* device = rhiDevice();
  if (device == nullptr || !water_renderer_.ready()) {
    return;
  }
  if (drawn_water_shape_ != water_.shapeCount()) {
    drawn_water_shape_ = water_.shapeCount();
    (void)water_renderer_.setSurface(*device,
                                     makeWaterSurfaceMesh(water_.layer()));
    (void)water_renderer_.setShape(*device, water_.field());
  }
  (void)water_renderer_.setField(*device, water_.field());
}

void SimplishEditor::drawWater(RhiCommandList& cmd,
                               const EditorViewportWidget& viewport) {
  const MeshRenderer::DrawParams scene = sceneDrawParams(viewport);
  WaterRenderer::DrawParams params{};
  params.view_projection = scene.view_projection;
  params.viewport = scene.viewport;
  params.scissor = scene.scissor;
  params.depth = sceneDepthTarget();
  params.fidelity = water_.fidelity();
  params.effects = state_.graphics.water_effects;
  params.seconds = water_.seconds();
  // Lit as the meshes around it are, cel-banded with them.
  params.lights = scene.lights;
  params.shade_bands = scene.shade_bands;
  water_renderer_.draw(cmd, params);
  state_.water.drawn = water_renderer_.drawable();
}

std::vector<GroundCell> SimplishEditor::selectedArea() const {
  const bool area =
      selectionIs(state_.selection, EditorSelectionKind::GROUND) ||
      selectionIs(state_.selection, EditorSelectionKind::WATER);
  return area ? state_.ground_selection : std::vector<GroundCell>{};
}

void SimplishEditor::showAreaSelection(EditorPropertiesWidget& panel) {
  if (selectionIs(state_.selection, EditorSelectionKind::WATER)) {
    showWaterSelection(panel);
  } else {
    showGroundSelection(panel);
  }
}

bool SimplishEditor::deleteSelectedArea() {
  if (selectedArea().empty()) {
    return false;
  }
  // An area of ground is erased: painted over with bare ground. A body of
  // water is dried, leaving the ground under it.
  if (selectionIs(state_.selection, EditorSelectionKind::WATER)) {
    drySelectedWater();
  } else {
    repaintSelectedGround(0);
  }
  return true;
}

bool SimplishEditor::selectAreaAt(WorldPoint tile) {
  const GroundCell cell{static_cast<int32_t>(std::floor(tile.x)),
                        static_cast<int32_t>(std::floor(tile.y))};
  // Water lies over the ground, so a click on it picks the water.
  if (selectEditorWater(state_, cell)) {
    if (const std::optional<WaterCell> water = editorSelectedWater(state_)) {
      brush_water_ = *water;
    }
    return true;
  }
  return selectEditorGround(state_, cell);
}

void SimplishEditor::showWaterSelection(EditorPropertiesWidget& panel) {
  const WaterCell water = editorSelectedWater(state_).value_or(WaterCell{});
  const GroundCell first = state_.ground_selection.front();
  const size_t tiles = state_.ground_selection.size();
  panel.setWaterSelection(
      "Water — " + std::to_string(tiles) + (tiles == 1 ? " tile" : " tiles"),
      "water:" + std::to_string(first.x) + "," + std::to_string(first.y),
      water);
  EditorWaterDepthChoices depths = editorWaterDepthChoices(
      state_.document.water.depth, state_.ground_selection);
  panel.addChoices(EditorChoiceKind::WATER_DEPTH, std::move(depths.names),
                   depths.current);
}

void SimplishEditor::applyWaterEdit(EditorPropertyField field, float value,
                                    EditorPropertyEdit edit) {
  // The first change of a gesture is what the eventual undo restores.
  if (!water_prior_) {
    water_prior_ = state_.document.water;
  }
  setEditorWaterValue(state_.document.water, state_.ground_selection, field,
                      value);
  if (edit == EditorPropertyEdit::COMMIT) {
    commitWaterEdit();
  }
}

void SimplishEditor::commitWaterEdit() {
  const std::optional<WaterLayer> prior =
      std::exchange(water_prior_, std::nullopt);
  if (!prior) {
    return;
  }
  std::vector<EditorWaterChange> changes =
      diffEditorWater(*prior, state_.document.water);
  if (!changes.empty()) {
    // Recorded against the document, which already holds the change.
    recordAction(
        {.kind = EditorActionKind::PAINT_GROUND, .water = std::move(changes)});
  }
  if (const std::optional<WaterCell> water = editorSelectedWater(state_)) {
    brush_water_ = *water;
  }
}

void SimplishEditor::deepenSelectedWater(size_t index) {
  if (isPlaying() || index >= EDITOR_WATER_DEPTH_COUNT ||
      !selectionIs(state_.selection, EditorSelectionKind::WATER)) {
    return;
  }
  WaterLayer deepened = state_.document.water;
  setEditorWaterDepth(deepened, state_.ground_selection,
                      waterDepthUnits(EDITOR_WATER_DEPTHS[index].tiles));
  if (const std::optional<EditorAction> action =
          editorWaterEdit(state_.document, deepened)) {
    recordAction(*action);
  }
  applySelectionToChrome();
}

void SimplishEditor::drySelectedWater() {
  if (isPlaying() ||
      !selectionIs(state_.selection, EditorSelectionKind::WATER)) {
    return;
  }
  WaterLayer dried = state_.document.water;
  for (const GroundCell cell : state_.ground_selection) {
    setWaterCell(dried, cell, WaterCell{.depth = 0});
  }
  state_.selection = {};
  if (const std::optional<EditorAction> action =
          editorWaterEdit(state_.document, dried)) {
    recordAction(*action);
  }
  applySelectionToChrome();
}

}  // namespace eng::editor
