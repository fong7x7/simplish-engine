#include "editor-vector-field.h"

#include <algorithm>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-property-traits.h>
#include <editor/shell/editor-sprite-ops.h>
#include <engine/render-sprite/sprite-sheet-frames.h>
#include <iterator>

namespace eng::editor {

namespace {

  using Field = EditorPropertyField;

  /// How fast a dropped billboard plays, in frames a second: the rate
  /// hand-drawn sheets are almost always authored at.
  constexpr float SPRITE_DEFAULT_FPS = 12.0f;

  /// What the Sheet row calls a sheet the project does not have.
  constexpr std::string_view MISSING_SUFFIX = " (missing)";

  bool isPositionField(Field field) {
    return editorFieldInTriple(field, Field::POSITION_X);
  }

  /// A sheet's path without its extension, which is what the Sheet row and
  /// the name line show: `sprites/slime.png` reads as `sprites/slime`.
  std::string sheetLabel(std::string_view sheet) {
    const std::filesystem::path path{std::string(sheet)};
    return (path.parent_path() / path.stem()).generic_string();
  }

  /// Whether @p sheets holds @p sheet, compared as the paths are written.
  bool holdsSheet(std::span<const std::filesystem::path> sheets,
                  std::string_view sheet) {
    return std::ranges::any_of(sheets, [sheet](const auto& path) {
      return path.generic_string() == sheet;
    });
  }

  /// Cells the grid of @p sprite holds.
  uint32_t gridCells(const EditorSprite& sprite) {
    return static_cast<uint32_t>(std::max<uint16_t>(sprite.grid.columns, 1)) *
           std::max<uint16_t>(sprite.grid.rows, 1);
  }

  /// Hold the frame count to a grid that has just changed shape: a sheet
  /// playing every cell goes on playing every cell, and one playing fewer
  /// keeps its own number, held to what the new grid has room for.
  void refitFrames(EditorSprite& sprite, uint32_t previous_cells) {
    const uint32_t cells = gridCells(sprite);
    const bool played_all =
        sprite.grid.frames == 0 || sprite.grid.frames >= previous_cells;
    sprite.grid.frames = static_cast<uint16_t>(
        played_all ? cells : std::min<uint32_t>(sprite.grid.frames, cells));
  }

  /// Write a whole-number field of the grid, keeping the frame count with
  /// it. `normalizeEditorPropertyValue` has already held @p value to the
  /// field's own range.
  void setGridValue(EditorSprite& sprite, Field field, float value) {
    const uint32_t cells = gridCells(sprite);
    const auto written = static_cast<uint16_t>(value);
    if (field == Field::COLUMNS) {
      sprite.grid.columns = written;
    } else {
      sprite.grid.rows = written;
    }
    refitFrames(sprite, cells);
  }

  /// The value of a field that is not one of the position's three; zero
  /// for a field a billboard has not got.
  float scalarValue(const EditorSprite& sprite, Field field) {
    switch (field) {
      case Field::HEIGHT:
        return sprite.height;
      case Field::COLUMNS:
        return static_cast<float>(sprite.grid.columns);
      case Field::ROWS:
        return static_cast<float>(sprite.grid.rows);
      case Field::FRAMES:
        // The number actually played, so a file that wrote none — meaning
        // every cell — shows the count it is playing rather than a zero.
        return static_cast<float>(spriteSheetFrameCount(sprite.grid));
      case Field::FPS:
        return sprite.grid.fps;
      default:
        return 0.0f;
    }
  }

}  // namespace

EditorSprite makeEditorSprite(std::string_view sheet, WorldPoint position) {
  EditorSprite sprite;
  sprite.sheet = std::string(sheet);
  sprite.position = position;
  sprite.grid = {1, 1, 1, SPRITE_DEFAULT_FPS};
  return sprite;
}

std::string editorSpriteName(const EditorSprite& sprite) {
  const std::string name(EDITOR_SPRITE_NAME);
  return sprite.sheet.empty() ? name : name + " · " + sheetLabel(sprite.sheet);
}

EditorSheetChoices
editorSheetChoices(const EditorSprite& sprite,
                   std::span<const std::filesystem::path> sheets) {
  EditorSheetChoices choices;
  for (const std::filesystem::path& path : sheets) {
    std::string written = path.generic_string();
    if (written == sprite.sheet) {
      choices.current = choices.paths.size();
    }
    choices.names.push_back(sheetLabel(written));
    choices.paths.push_back(std::move(written));
  }
  if (!sprite.sheet.empty() && !holdsSheet(sheets, sprite.sheet)) {
    choices.current = choices.paths.size();
    choices.names.push_back(sheetLabel(sprite.sheet) +
                            std::string(MISSING_SUFFIX));
    choices.paths.push_back(sprite.sheet);
  }
  return choices;
}

float editorSpriteValue(const EditorSprite& sprite, Field field) {
  if (isPositionField(field)) {
    return editorVectorValue(sprite.position,
                             editorFieldAxis(field, Field::POSITION_X));
  }
  return scalarValue(sprite, field);
}

void setEditorSpriteValue(EditorSprite& sprite, Field field, float value) {
  const float written = normalizeEditorPropertyValue(field, value);
  if (isPositionField(field)) {
    editorVectorAxis(sprite.position,
                     editorFieldAxis(field, Field::POSITION_X)) = written;
  } else if (field == Field::HEIGHT) {
    sprite.height = written;
  } else if (field == Field::COLUMNS || field == Field::ROWS) {
    setGridValue(sprite, field, written);
  } else if (field == Field::FRAMES) {
    sprite.grid.frames = static_cast<uint16_t>(
        std::min<uint32_t>(static_cast<uint32_t>(written), gridCells(sprite)));
  } else if (field == Field::FPS) {
    sprite.grid.fps = written;
  }
}

bool editorSpriteHasField(Field field) {
  return std::ranges::find(EDITOR_SPRITE_FIELDS, field) !=
         std::end(EDITOR_SPRITE_FIELDS);
}

bool sameEditorSprite(const EditorSprite& a, const EditorSprite& b) {
  return a.id == b.id && a.sheet == b.sheet && a.height == b.height &&
         a.position.x == b.position.x && a.position.y == b.position.y &&
         a.position.z == b.position.z && a.grid.columns == b.grid.columns &&
         a.grid.rows == b.grid.rows && a.grid.frames == b.grid.frames &&
         a.grid.fps == b.grid.fps;
}

PlacementBounds editorSpriteBounds(const EditorSprite& sprite, float width) {
  const WorldPoint& at = sprite.position;
  const float half = std::max(width, 0.0f) * 0.5f;
  const float depth = EDITOR_SPRITE_MARKER_DEPTH * 0.5f;
  return {{at.x - half, at.y - depth, at.z},
          {at.x + half, at.y + depth, at.z + sprite.height}};
}

}  // namespace eng::editor
