#pragma once

/// @file editor-sprite-ops.h
/// @brief Make, read, write and name sprite billboards.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-placement-bounds.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-sheet-choices.h>
#include <editor/shell/editor-sprite.h>
#include <editor/shell/iso-projection.h>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The definition a billboard is saved under, in a level file's `entities`
/// list.
inline constexpr std::string_view EDITOR_SPRITE_DEFINITION =
    "entity:sprite_billboard";

/// What the browser and the panel call a billboard.
inline constexpr std::string_view EDITOR_SPRITE_NAME = "Sprite Billboard";

/// How high above the floor a dropped billboard's base stands, in tiles:
/// none. A sprite's feet are on the ground, which is where its depth has
/// to be measured for it to sort against what it is standing among.
inline constexpr float EDITOR_SPRITE_DROP_HEIGHT = 0.0f;

/// How deep the box a billboard is drawn and picked as is, in tiles.
///
/// A quad has no depth at all, and a marker with none would be a line the
/// pointer could not hit. A tenth of a tile is enough to read as a
/// footprint and little enough that it never reaches the next tile.
inline constexpr float EDITOR_SPRITE_MARKER_DEPTH = 0.1f;

/// A new billboard standing at @p position, showing @p sheet — which may
/// be empty, for a project with no sheets in it yet.
///
/// One frame, playing at the default speed: a sheet is a grid only its
/// author knows the shape of, and a billboard that started by guessing
/// would show a quarter of a frame until it was corrected. One frame is
/// the whole image, which is right for a still and readable for a sheet.
[[nodiscard]] EditorSprite makeEditorSprite(std::string_view sheet,
                                            WorldPoint position);

/// The name line the panel shows: `Sprite Billboard · slime`, or just the
/// kind for one with no sheet.
[[nodiscard]] std::string editorSpriteName(const EditorSprite& sprite);

/// What the Sheet row offers @p sprite: every sheet in @p sheets, by path
/// without its extension. A sheet it names that the project does not have
/// — a file deleted since, or a hand-edited path — is offered last, as
/// `<path> (missing)`, so the row can say so rather than silently
/// repointing the billboard at something else.
[[nodiscard]] EditorSheetChoices
editorSheetChoices(const EditorSprite& sprite,
                   std::span<const std::filesystem::path> sheets);

/// Current value of one of a billboard's properties; zero for a field it
/// does not have. The frame count reads as the number actually played,
/// which is what `spriteSheetFrameCount` settles.
[[nodiscard]] float editorSpriteValue(const EditorSprite& sprite,
                                      EditorPropertyField field);

/// Write one of a billboard's properties, normalised as
/// `normalizeEditorPropertyValue` defines. A field it does not have is
/// ignored.
///
/// Changing the grid carries the frame count with it: a sheet playing
/// every cell it has goes on playing every cell, and one playing fewer is
/// held to what the new grid holds. Nothing else moves a number the
/// designer set.
void setEditorSpriteValue(EditorSprite& sprite, EditorPropertyField field,
                          float value);

/// Whether @p field is one a billboard has: `EDITOR_SPRITE_FIELDS`.
[[nodiscard]] bool editorSpriteHasField(EditorPropertyField field);

/// Whether two billboards are the same to the last bit — the test for
/// "this edit changed nothing", which then records nothing.
[[nodiscard]] bool sameEditorSprite(const EditorSprite& a,
                                    const EditorSprite& b);

/// The box @p sprite is drawn and picked as: @p width tiles across, its
/// own height tall, and `EDITOR_SPRITE_MARKER_DEPTH` deep about its base.
[[nodiscard]] PlacementBounds editorSpriteBounds(const EditorSprite& sprite,
                                                 float width);

}  // namespace eng::editor
