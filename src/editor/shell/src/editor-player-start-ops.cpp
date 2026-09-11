#include "editor-vector-field.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-property-traits.h>

namespace eng::editor {

namespace {

  /// Whether a field names one of a start's position components.
  bool isPositionField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::POSITION_X);
  }

  static_assert(editorPropertyTraits(EditorPropertyField::PLAYER).maximum ==
                    static_cast<float>(EDITOR_PLAYER_SLOTS),
                "the Player row holds exactly the players a session has");

}  // namespace

uint8_t clampEditorPlayerSlot(float player) {
  // A NaN compares false with everything and would survive the clamp as
  // itself; it is no player at all, so it is the first one.
  if (std::isnan(player)) {
    return 1;
  }
  const float rounded = std::round(player);
  return static_cast<uint8_t>(
      std::clamp(rounded, 1.0f, static_cast<float>(EDITOR_PLAYER_SLOTS)));
}

uint8_t nextEditorPlayerSlot(const EditorDocument& document) {
  std::array<bool, EDITOR_PLAYER_SLOTS> taken{};
  for (const EditorPlayerStart& start : document.player_starts) {
    if (start.player >= 1 && start.player <= EDITOR_PLAYER_SLOTS) {
      taken[start.player - 1U] = true;
    }
  }
  for (uint8_t slot = 0; slot < EDITOR_PLAYER_SLOTS; ++slot) {
    if (!taken[slot]) {
      return static_cast<uint8_t>(slot + 1U);
    }
  }
  return 1;
}

EditorPlayerStart makeEditorPlayerStart(uint8_t player, WorldPoint position) {
  EditorPlayerStart start;
  start.player = clampEditorPlayerSlot(static_cast<float>(player));
  start.position = position;
  return start;
}

std::string editorPlayerStartName(const EditorPlayerStart& start) {
  return "Player " + std::to_string(start.player) + " Start";
}

float editorPlayerStartValue(const EditorPlayerStart& start,
                             EditorPropertyField field) {
  if (isPositionField(field)) {
    return editorVectorValue(
        start.position,
        editorFieldAxis(field, EditorPropertyField::POSITION_X));
  }
  return field == EditorPropertyField::PLAYER ? static_cast<float>(start.player)
                                              : 0.0f;
}

void setEditorPlayerStartValue(EditorPlayerStart& start,
                               EditorPropertyField field, float value) {
  const float written = normalizeEditorPropertyValue(field, value);
  if (isPositionField(field)) {
    editorVectorAxis(start.position,
                     editorFieldAxis(field, EditorPropertyField::POSITION_X)) =
        written;
  } else if (field == EditorPropertyField::PLAYER) {
    start.player = clampEditorPlayerSlot(written);
  }
}

bool editorPlayerStartHasField(EditorPropertyField field) {
  return std::ranges::find(EDITOR_PLAYER_START_FIELDS, field) !=
         std::end(EDITOR_PLAYER_START_FIELDS);
}

PlacementBounds editorPlayerStartBounds(const EditorPlayerStart& start) {
  const WorldPoint& at = start.position;
  const float reach = EDITOR_PLAYER_START_MARKER_RADIUS;
  return {
      {at.x - reach, at.y - reach, at.z},
      {at.x + reach, at.y + reach, at.z + EDITOR_PLAYER_START_MARKER_HEIGHT}};
}

}  // namespace eng::editor
