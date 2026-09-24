#include <algorithm>
#include <editor/shell/editor-ground-atlas.h>
#include <editor/shell/editor-shell-selection.h>
#include <editor/shell/editor-terrains.h>
#include <editor/shell/editor-water-depths.h>
#include <editor/shell/editor-water-ops.h>
#include <editor/shell/simplish-editor.h>
#include <engine/core/logger.h>
#include <engine/math/mat4.h>
#include <engine/render-ground/ground-mesh.h>
#include <engine/render-water/water-depth.h>
#include <engine/render/rhi-texture-desc.h>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  /// The key that shrinks the brush.
  constexpr uint32_t KEY_SMALLER = '[';
  /// The key that grows it.
  constexpr uint32_t KEY_LARGER = ']';
  /// The key that makes the water the brush paints shallower.
  constexpr uint32_t KEY_SHALLOWER = ',';
  /// The key that makes it deeper.
  constexpr uint32_t KEY_DEEPER = '.';

  /// How the atlas is described to the device: a mesh's diffuse map, Unorm
  /// for the reason every mesh texture is (see `MeshRenderer`).
  RhiTextureDesc atlasDesc(const ImageData& image) {
    RhiTextureDesc desc{};
    desc.width = image.width;
    desc.height = image.height;
    desc.format = RhiFormat::RGB_A8_UNORM;
    desc.usage = RhiTextureUsage::SAMPLED;
    desc.debug_name = "ground-atlas";
    desc.initial_pixels = image.pixels.data();
    return desc;
  }

}  // namespace

void SimplishEditor::pickGroundCard(size_t card, WorldPoint tile) {
  brush_kind_ = editorGroundCardBrush(card);
  brush_terrain_ = editorGroundCardTerrain(card);
  state_.active_tool = EditorTool::TILE_PAINT;
  // The card lands as one dab, recorded as a stroke of its own, so the drop
  // shows what the brush now paints and one undo takes it back.
  paintStroke(EditorStrokePhase::BEGIN, {tile.x + 0.5f, tile.y + 0.5f});
  paintStroke(EditorStrokePhase::END, {tile.x + 0.5f, tile.y + 0.5f});
}

void SimplishEditor::paintStroke(EditorStrokePhase phase, WorldPoint point) {
  if (phase == EditorStrokePhase::END) {
    endStroke();
    return;
  }
  // Nothing is painted into a level while it is being played; the stroke
  // simply never starts.
  if (isPlaying() || (phase == EditorStrokePhase::MOVE && !stroke_before_)) {
    return;
  }
  if (phase == EditorStrokePhase::BEGIN) {
    commitPendingEdit();
    stroke_before_ = state_.document.ground;
    stroke_water_before_ = state_.document.water;
  }
  paintBrushAt(point);
}

void SimplishEditor::paintBrushAt(WorldPoint point) {
  const GroundRect rect = editorBrushRect(point, brush_size_);
  if (brush_kind_ == EditorGroundBrush::TERRAIN) {
    paintEditorGround(state_.document.ground, rect, brush_terrain_);
  } else if (brush_kind_ == EditorGroundBrush::DRY) {
    dryEditorWater(state_.document.water, rect);
  } else {
    // Water is laid at the brush's depth — so painting over a pond with a
    // lake's depth deepens it where the brush passed, and the tiles between
    // blend — and, where it was dry, in the brush's colour.
    WaterCell water = brush_water_;
    water.depth = waterDepthUnits(EDITOR_WATER_DEPTHS[brush_depth_].tiles);
    layEditorWater(state_.document.water, rect, water);
  }
}

void SimplishEditor::endStroke() {
  const std::optional<GroundGrid> before =
      std::exchange(stroke_before_, std::nullopt);
  const std::optional<WaterLayer> water_before =
      std::exchange(stroke_water_before_, std::nullopt);
  if (!before || !water_before) {
    return;
  }
  EditorAction action{
      .kind = EditorActionKind::PAINT_GROUND,
      .ground = diffEditorGround(*before, state_.document.ground),
      .water = diffEditorWater(*water_before, state_.document.water)};
  // A stroke that painted only what was already there is no edit. The
  // document already holds the stroke, so the action is recorded against
  // it rather than applied over it — applying it again writes the same
  // cells.
  if (!action.ground.empty() || !action.water.empty()) {
    recordAction(action);
  }
}

bool SimplishEditor::handleBrushKey(uint32_t key) {
  if (state_.active_tool != EditorTool::TILE_PAINT) {
    return false;
  }
  if (key == KEY_SHALLOWER || key == KEY_DEEPER) {
    const size_t deepest = EDITOR_WATER_DEPTH_COUNT - 1;
    brush_depth_ = key == KEY_DEEPER  ? std::min(brush_depth_ + 1, deepest)
                   : brush_depth_ > 0 ? brush_depth_ - 1
                                      : 0;
    return true;
  }
  if (key != KEY_SMALLER && key != KEY_LARGER) {
    return false;
  }
  brush_size_ = std::clamp(brush_size_ + (key == KEY_LARGER ? 1 : -1),
                           EDITOR_BRUSH_SIZE_MIN, EDITOR_BRUSH_SIZE_MAX);
  return true;
}

void SimplishEditor::applyBrushToViewport(
    EditorViewportWidget& viewport) const {
  viewport.paints =
      state_.active_tool == EditorTool::TILE_PAINT && !isPlaying();
  viewport.brush_size = brush_size_;
}

std::string SimplishEditor::brushStatus() const {
  const std::string side = std::to_string(brush_size_);
  // Water says how deep it lays down, and which keys change that.
  const std::string depth =
      brush_kind_ == EditorGroundBrush::WATER
          ? ", " + std::string(EDITOR_WATER_DEPTHS[brush_depth_].name) +
                " depth (, .)"
          : std::string{};
  return "   brush " + std::string(editorGroundCardName(brushCard())) + " " +
         side + "x" + side + depth;
}

void SimplishEditor::refreshGroundMesh() {
  RhiDevice* device = rhiDevice();
  if (state_.document.ground == drawn_ground_ || device == nullptr ||
      !mesh_renderer_.ready()) {
    return;
  }
  drawn_ground_ = state_.document.ground;
  if (ground_mesh_ != MESH_GPU_INVALID) {
    mesh_renderer_.release(*device, ground_mesh_);
    ground_mesh_ = MESH_GPU_INVALID;
  }
  const MeshData mesh =
      makeGroundMesh(drawn_ground_, static_cast<uint8_t>(EDITOR_TERRAIN_COUNT));
  if (!mesh.vertices.empty()) {
    ground_mesh_ =
        mesh_renderer_.upload(*device, mesh).value_or(MESH_GPU_INVALID);
  }
}

void SimplishEditor::appendGroundInstance() {
  if (ground_mesh_ == MESH_GPU_INVALID) {
    return;
  }
  RhiDevice* device = rhiDevice();
  if (ground_atlas_ == RHI_TEXTURE_INVALID && device != nullptr) {
    const ImageData atlas = makeEditorGroundAtlas();
    ground_atlas_ = device->createTexture(atlasDesc(atlas));
  }
  // Built in world coordinates, so it is drawn where it was painted.
  scene_instances_.push_back({ground_mesh_, Mat4::identity(), ground_atlas_});
}

void SimplishEditor::releaseGround() {
  RhiDevice* device = rhiDevice();
  if (device != nullptr && ground_mesh_ != MESH_GPU_INVALID) {
    mesh_renderer_.release(*device, ground_mesh_);
  }
  if (device != nullptr && ground_atlas_ != RHI_TEXTURE_INVALID) {
    device->destroyTexture(ground_atlas_);
  }
  ground_mesh_ = MESH_GPU_INVALID;
  ground_atlas_ = RHI_TEXTURE_INVALID;
  drawn_ground_ = GroundGrid{};
}

void SimplishEditor::showGroundSelection(EditorPropertiesWidget& panel) {
  const uint8_t terrain = editorSelectedTerrain(state_).value_or(0);
  const GroundCell first = state_.ground_selection.front();
  const size_t tiles = state_.ground_selection.size();
  panel.setGroundSelection(
      std::string(editorGroundCardName(terrain == 0 ? EDITOR_TERRAIN_COUNT
                                                    : terrain - 1U)) +
          " — " + std::to_string(tiles) + (tiles == 1 ? " tile" : " tiles"),
      "ground:" + std::to_string(first.x) + "," + std::to_string(first.y));
  std::vector<std::string> terrains;
  for (size_t card = 0; card < EDITOR_TERRAIN_CHOICE_COUNT; ++card) {
    terrains.emplace_back(editorGroundCardName(card));
  }
  panel.addChoices(EditorChoiceKind::TERRAIN, std::move(terrains),
                   terrain == 0 ? EDITOR_TERRAIN_COUNT : terrain - 1U);
}

size_t SimplishEditor::brushCard() const {
  if (brush_kind_ != EditorGroundBrush::TERRAIN) {
    return brush_kind_ == EditorGroundBrush::WATER ? EDITOR_WATER_CARD
                                                   : EDITOR_DRY_CARD;
  }
  return brush_terrain_ == 0 ? EDITOR_TERRAIN_COUNT : brush_terrain_ - 1U;
}

void SimplishEditor::repaintSelectedGround(uint8_t terrain) {
  if (isPlaying() ||
      !selectionIs(state_.selection, EditorSelectionKind::GROUND)) {
    return;
  }
  const std::optional<EditorAction> action =
      editorGroundRepaint(state_.document, state_.ground_selection, terrain);
  // Erased is gone, so nothing is left to show; any other terrain leaves
  // the same cells selected, now painted with it.
  if (terrain == 0) {
    state_.selection = {};
  }
  if (action) {
    recordAction(*action);
  }
  applySelectionToChrome();
}

}  // namespace eng::editor
