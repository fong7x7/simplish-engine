#pragma once

/// @file editor-player-start-ops.h
/// @brief Make, read, and write the points players enter a level at.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstdint>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-placement-bounds.h>
#include <editor/shell/editor-player-start.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/iso-projection.h>
#include <engine/sim/tick-input.h>
#include <string>
#include <string_view>

namespace eng::editor {

/// Players a session holds, and so the highest player a start can name.
/// The simulation's own number, so the editor cannot author a start for a
/// player the game has no input slot for.
inline constexpr uint8_t EDITOR_PLAYER_SLOTS =
    static_cast<uint8_t>(sim::MAX_PLAYERS);

/// The definition a player start is saved under, in a level file's
/// `entities` list ([project-format.md §4]).
inline constexpr std::string_view EDITOR_PLAYER_START_DEFINITION =
    "entity:player_start";

/// Half the width of the column a player start is drawn and picked as.
inline constexpr float EDITOR_PLAYER_START_MARKER_RADIUS = 0.3f;

/// How tall that column stands, in tiles: about a person, so a start reads
/// as somebody standing on the tile rather than as a crate.
inline constexpr float EDITOR_PLAYER_START_MARKER_HEIGHT = 1.5f;

/// What the browser and the panel call a player start.
inline constexpr std::string_view EDITOR_PLAYER_START_NAME = "Player Start";

/// @p player held to a slot a session has: rounded to the nearest whole
/// player, then clamped into [1, `EDITOR_PLAYER_SLOTS`].
[[nodiscard]] uint8_t clampEditorPlayerSlot(float player);

/// The lowest player no start in @p document names yet, so dropping four
/// starts one after another gives players one to four. Player 1 once every
/// player has one: a second start for a player is a legitimate thing to
/// author, and the lowest is the least surprising to hand out.
[[nodiscard]] uint8_t nextEditorPlayerSlot(const EditorDocument& document);

/// A new start for @p player with its feet at @p position.
[[nodiscard]] EditorPlayerStart makeEditorPlayerStart(uint8_t player,
                                                      WorldPoint position);

/// The name line the panel shows for @p start: `Player 2 Start`.
[[nodiscard]] std::string editorPlayerStartName(const EditorPlayerStart& start);

/// Current value of one of a start's properties. A field a start does not
/// have — a rotation, a light's range — reads as zero.
[[nodiscard]] float editorPlayerStartValue(const EditorPlayerStart& start,
                                           EditorPropertyField field);

/// Write one of a start's properties, normalised as
/// `normalizeEditorPropertyValue` defines. A field a start does not have is
/// ignored.
void setEditorPlayerStartValue(EditorPlayerStart& start,
                               EditorPropertyField field, float value);

/// Whether @p field is one a player start has: its position and its player.
[[nodiscard]] bool editorPlayerStartHasField(EditorPropertyField field);

/// The column a start is drawn and picked as, standing on its position.
[[nodiscard]] PlacementBounds
editorPlayerStartBounds(const EditorPlayerStart& start);

}  // namespace eng::editor
