// SimplishEditor's sprite billboards: placing one from general > sprites,
// the Sheet row and the grid's numbers in the properties panel, and drawing
// each one as a camera-facing quad showing the frame the render clock has
// reached. ADR-003's sprite half, through the mesh pipeline's alpha-test
// cutout.

#include <editor/project/project-paths.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-sprite-transform.h>
#include <editor/shell/simplish-editor.h>
#include <engine/core/logger.h>
#include <engine/gui/image-loader.h>
#include <engine/render-sprite/sprite-quad.h>
#include <engine/render-sprite/sprite-sheet-frames.h>
#include <engine/render/rhi-texture-desc.h>
#include <utility>

namespace eng::editor {

namespace {

  /// How many billboard quads the editor keeps before starting over.
  ///
  /// A quad is four vertices, so the memory is nothing; what this bounds is
  /// the number of GPU buffers a long session can accumulate as grids are
  /// re-cut in the panel. Past it the whole cache is released at the start
  /// of the next frame and refilled with the frames actually being shown,
  /// which costs one upload each.
  constexpr size_t SPRITE_QUAD_LIMIT = 512;

  /// How a sheet image is described to the device.
  ///
  /// Unorm rather than sRGB, for the reason a mesh's diffuse map is: the
  /// mesh shader converts on the way out, and a texture the GPU decoded on
  /// sample would be converted twice.
  RhiTextureDesc sheetTextureDesc(const ImageData& image) {
    RhiTextureDesc desc{};
    desc.width = image.width;
    desc.height = image.height;
    desc.format = RhiFormat::RGB_A8_UNORM;
    desc.usage = RhiTextureUsage::SAMPLED;
    desc.debug_name = "sprite-sheet";
    desc.initial_pixels = image.pixels.data();
    return desc;
  }

}  // namespace

void SimplishEditor::placeSprite(WorldPoint tile) {
  // Feet on the middle of the tile it was dropped on: a sprite's base is
  // where its depth is measured, so it has to be on the floor it stands on.
  const WorldPoint position{tile.x + 0.5f, tile.y + 0.5f,
                            EDITOR_SPRITE_DROP_HEIGHT};
  // The project's first sheet, so a billboard dropped into a project that
  // has any art shows some of it at once; a project with none drops an
  // empty one, and its Sheet row says so.
  const std::string sheet = state_.sheets.empty()
                                ? std::string{}
                                : state_.sheets.front().generic_string();
  const size_t added = state_.document.sprites.size();
  EditorSprite sprite = makeEditorSprite(sheet, position);
  sprite.id = mintEditorSpriteId(state_.document);
  recordAction(
      {.kind = EditorActionKind::ADD_SPRITE, .index = added, .sprite = sprite});
  select({EditorSelectionKind::SPRITE, added});
}

void SimplishEditor::showSpriteSelection(EditorPropertiesWidget& panel) {
  const EditorSprite& sprite = state_.document.sprites[state_.selection.index];
  panel.setSelection(editorSpriteName(sprite), sprite);
  EditorSheetChoices choices = editorSheetChoices(sprite, state_.sheets);
  const size_t current = choices.current;
  panel.addChoices(EditorChoiceKind::SHEET, std::move(choices.names), current);
}

void SimplishEditor::applySpriteEdit(EditorPropertyField field, float value,
                                     EditorPropertyEdit edit) {
  EditorSprite& sprite = state_.document.sprites[state_.selection.index];
  if (!sprite_prior_.has_value()) {
    sprite_prior_ = sprite;
  }
  setEditorSpriteValue(sprite, field, value);
  if (edit == EditorPropertyEdit::COMMIT) {
    commitSpriteEdit();
  }
}

void SimplishEditor::commitSpriteEdit() {
  if (!sprite_prior_.has_value()) {
    return;
  }
  const auto prior = *std::exchange(sprite_prior_, std::nullopt);
  if (!editSubjectSelected(EditorSelectionKind::SPRITE)) {
    return;
  }
  const auto& sprite = state_.document.sprites[state_.selection.index];
  if (sameEditorSprite(prior, sprite)) {
    return;
  }
  recordAction({.kind = EditorActionKind::TRANSFORM_SPRITE,
                .index = state_.selection.index,
                .sprite = sprite,
                .sprite_prior = prior});
}

void SimplishEditor::applySheetChoice(size_t index) {
  if (isPlaying() || !editSubjectSelected(EditorSelectionKind::SPRITE)) {
    return;
  }
  EditorSprite& sprite = state_.document.sprites[state_.selection.index];
  const EditorSheetChoices choices = editorSheetChoices(sprite, state_.sheets);
  if (index >= choices.paths.size()) {
    return;
  }
  if (!sprite_prior_.has_value()) {
    sprite_prior_ = sprite;
  }
  sprite.sheet = choices.paths[index];
  commitSpriteEdit();
}

EditorPlacementMarker SimplishEditor::spriteMarker(size_t index) {
  const EditorSprite& sprite = state_.document.sprites[index];
  return {editorSpriteBounds(sprite, spriteWidth(sprite)),
          isSelected(EditorSelectionKind::SPRITE, index),
          EditorMarkerStyle::SPRITE};
}

float SimplishEditor::spriteWidth(const EditorSprite& sprite) {
  const EditorSpriteSheetTexture* sheet = ensureSpriteSheet(sprite.sheet);
  if (sheet == nullptr) {
    return sprite.height;
  }
  return editorSpriteWidth(projectionAxes(), sprite,
                           spriteFramePixels(sprite.grid, sheet->pixels));
}

void SimplishEditor::appendSpriteInstances() {
  // Between frames rather than part-way through one: a quad released while
  // an instance already appended named it would leave that billboard
  // undrawn for a frame.
  if (sprite_quads_.size() >= SPRITE_QUAD_LIMIT) {
    releaseSpriteQuads();
  }
  for (const EditorSprite& sprite : state_.document.sprites) {
    appendSpriteInstance(sprite);
  }
}

void SimplishEditor::appendSpriteInstance(const EditorSprite& sprite) {
  const EditorSpriteSheetTexture* sheet = ensureSpriteSheet(sprite.sheet);
  if (sheet == nullptr) {
    return;
  }
  const uint16_t frame = spriteFrameAt(sprite.grid, animation_clock_);
  const MeshGpuId quad = ensureSpriteQuad(
      makeEditorSpriteQuadKey(sprite.grid, frame, sheet->pixels));
  if (quad == MESH_GPU_INVALID) {
    return;
  }
  const float width = editorSpriteWidth(
      projectionAxes(), sprite, spriteFramePixels(sprite.grid, sheet->pixels));
  scene_instances_.push_back(
      {quad, makeSpriteTransform(projectionAxes(), sprite, width),
       sheet->texture});
}

const EditorSpriteSheetTexture*
SimplishEditor::ensureSpriteSheet(const std::string& path) {
  RhiDevice* device = rhiDevice();
  if (path.empty() || device == nullptr) {
    return nullptr;
  }
  const auto found = sprite_sheets_.find(path);
  if (found != sprite_sheets_.end()) {
    // An entry with no texture is a file already found to be unreadable,
    // remembered so it is not read again on every frame.
    return found->second.texture == RHI_TEXTURE_INVALID ? nullptr
                                                        : &found->second;
  }
  return loadSpriteSheet(path);
}

const EditorSpriteSheetTexture*
SimplishEditor::loadSpriteSheet(const std::string& path) {
  const std::filesystem::path file =
      projectAssetsPath(state_.project.root) / path;
  const std::optional<ImageData> image =
      ImageLoader::loadFromFile(file.string());
  EditorSpriteSheetTexture sheet{};
  if (!image.has_value() || image->pixels.empty()) {
    LOG_WARN("editor", "Could not load sprite sheet: " + file.string());
  } else {
    sheet.texture = rhiDevice()->createTexture(sheetTextureDesc(*image));
    sheet.pixels = {static_cast<float>(image->width),
                    static_cast<float>(image->height)};
  }
  const auto added = sprite_sheets_.emplace(path, sheet).first;
  return sheet.texture == RHI_TEXTURE_INVALID ? nullptr : &added->second;
}

MeshGpuId SimplishEditor::ensureSpriteQuad(const EditorSpriteQuadKey& key) {
  const auto found = sprite_quads_.find(key);
  return found != sprite_quads_.end() ? found->second : uploadSpriteQuad(key);
}

MeshGpuId SimplishEditor::uploadSpriteQuad(const EditorSpriteQuadKey& key) {
  if (!mesh_renderer_.ready()) {
    return MESH_GPU_INVALID;
  }
  const auto uploaded = mesh_renderer_.upload(
      *rhiDevice(), makeSpriteQuadMesh(editorSpriteQuadUv(key)));
  if (!uploaded.has_value()) {
    return MESH_GPU_INVALID;
  }
  sprite_quads_.emplace(key, *uploaded);
  return *uploaded;
}

void SimplishEditor::releaseSpriteQuads() {
  RhiDevice* device = rhiDevice();
  for (const auto& [key, mesh] : sprite_quads_) {
    static_cast<void>(key);
    if (device != nullptr) {
      mesh_renderer_.release(*device, mesh);
    }
  }
  sprite_quads_.clear();
}

void SimplishEditor::releaseSpriteCache() {
  RhiDevice* device = rhiDevice();
  for (const auto& [path, sheet] : sprite_sheets_) {
    static_cast<void>(path);
    if (device != nullptr && sheet.texture != RHI_TEXTURE_INVALID) {
      device->destroyTexture(sheet.texture);
    }
  }
  sprite_sheets_.clear();
  releaseSpriteQuads();
}

}  // namespace eng::editor
