#include "editor-vector-field.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <iterator>

namespace eng::editor {

namespace {

  constexpr float FULL_TURN = 360.0f;
  constexpr float HALF_TURN = 180.0f;
  /// Enough for "-180.0" and a terminator several times over.
  constexpr size_t VALUE_TEXT_CAPACITY = 32;

  /// Wrap an angle into [-180, 180).
  float wrapDegrees(float degrees) {
    const float wrapped = std::fmod(degrees + HALF_TURN, FULL_TURN);
    return (wrapped < 0.0f ? wrapped + FULL_TURN : wrapped) - HALF_TURN;
  }

  /// Write @p value into @p text as @p field is shown: a whole number for a
  /// player, one decimal for an angle, two for everything else. Returns
  /// what snprintf returns.
  int writeValue(std::array<char, VALUE_TEXT_CAPACITY>& text, float value,
                 EditorPropertyField field) {
    if (editorPropertyFieldIsToggle(field)) {
      return std::snprintf(text.data(), text.size(), "%s",
                           value != 0.0f ? "on" : "off");
    }
    if (editorPropertyFieldKind(field) == EditorPropertyKind::SLOT) {
      return std::snprintf(text.data(), text.size(), "%.0f", value);
    }
    if (editorPropertyFieldIsAngle(field)) {
      return std::snprintf(text.data(), text.size(), "%.1f", value);
    }
    return std::snprintf(text.data(), text.size(), "%.2f", value);
  }

  /// How one kind of number moves under a step button and a drag.
  struct KindTuning {
    float step;
    float drag_per_pixel;
  };

  /// Every kind's tuning, indexed by `EditorPropertyKind`. A toggle has
  /// nothing to scrub — it is clicked, never dragged — so it drags nowhere.
  constexpr KindTuning KIND_TUNING[] = {
      {EDITOR_POSITION_STEP, EDITOR_POSITION_DRAG_PER_PIXEL},  // DISTANCE
      {EDITOR_POSITION_STEP, EDITOR_POSITION_DRAG_PER_PIXEL},  // EXTENT
      {EDITOR_ROTATION_STEP, EDITOR_ROTATION_DRAG_PER_PIXEL},  // ANGLE
      {EDITOR_AXIS_STEP, EDITOR_AXIS_DRAG_PER_PIXEL},          // AXIS
      {EDITOR_FACTOR_STEP, EDITOR_FACTOR_DRAG_PER_PIXEL},      // FACTOR
      {EDITOR_UNIT_STEP, EDITOR_UNIT_DRAG_PER_PIXEL},          // UNIT
      {EDITOR_SLOT_STEP, EDITOR_SLOT_DRAG_PER_PIXEL},          // SLOT
      {EDITOR_SLOT_STEP, 0.0f},                                // TOGGLE
      // A scale is set by where on its slider it is pressed, never stepped
      // or scrubbed — see `editor-scale-slider.h`.
      {0.0f, 0.0f},  // SCALE
  };

  static_assert(std::size(KIND_TUNING) ==
                    static_cast<size_t>(EditorPropertyKind::SCALE) + 1,
                "every property kind needs a step and a drag rate");

  const KindTuning& kindTuning(EditorPropertyKind kind) {
    return KIND_TUNING[static_cast<size_t>(kind)];
  }

  /// Whether a field names one of a placement's position components.
  bool isPositionField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::POSITION_X);
  }

  /// Whether a field names one of a placement's rotation components.
  bool isRotationField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::ROTATION_X);
  }

  /// A placement field that is one number of its own rather than part of a
  /// vector: whether it collides, and its scale.
  float placementScalarValue(const EditorPlacement& placement,
                             EditorPropertyField field) {
    if (field == EditorPropertyField::COLLIDES) {
      return placement.collides ? 1.0f : 0.0f;
    }
    if (field == EditorPropertyField::SCALE) {
      return placement.scale;
    }
    return 0.0f;
  }

}  // namespace

float normalizeEditorPropertyValue(EditorPropertyField field, float value) {
  switch (editorPropertyFieldKind(field)) {
    case EditorPropertyKind::ANGLE:
      return wrapDegrees(value);
    case EditorPropertyKind::AXIS:
      return std::clamp(value, -1.0f, 1.0f);
    case EditorPropertyKind::EXTENT:
    case EditorPropertyKind::FACTOR:
      return std::max(0.0f, value);
    case EditorPropertyKind::UNIT:
      return std::clamp(value, 0.0f, 1.0f);
    case EditorPropertyKind::SLOT:
      return static_cast<float>(clampEditorPlayerSlot(value));
    case EditorPropertyKind::TOGGLE:
      return value >= 0.5f ? 1.0f : 0.0f;
    case EditorPropertyKind::SCALE:
      return std::clamp(value, EDITOR_SCALE_MIN, EDITOR_SCALE_MAX);
    case EditorPropertyKind::DISTANCE:
      return value;
  }
  return value;
}

float editorPropertyValue(const EditorPlacement& placement,
                          EditorPropertyField field) {
  if (isPositionField(field)) {
    return editorVectorValue(
        placement.position,
        editorFieldAxis(field, EditorPropertyField::POSITION_X));
  }
  if (isRotationField(field)) {
    return editorVectorValue(
        placement.rotation,
        editorFieldAxis(field, EditorPropertyField::ROTATION_X));
  }
  return placementScalarValue(placement, field);
}

void setEditorPropertyValue(EditorPlacement& placement,
                            EditorPropertyField field, float value) {
  const float written = normalizeEditorPropertyValue(field, value);
  if (isPositionField(field)) {
    editorVectorAxis(placement.position,
                     editorFieldAxis(field, EditorPropertyField::POSITION_X)) =
        written;
  } else if (isRotationField(field)) {
    editorVectorAxis(placement.rotation,
                     editorFieldAxis(field, EditorPropertyField::ROTATION_X)) =
        written;
  } else if (field == EditorPropertyField::COLLIDES) {
    placement.collides = written != 0.0f;
  } else if (field == EditorPropertyField::SCALE) {
    placement.scale = written;
  }
}

float editorPropertyStep(EditorPropertyField field) {
  return kindTuning(editorPropertyFieldKind(field)).step;
}

float editorPropertyDragPerPixel(EditorPropertyField field) {
  return kindTuning(editorPropertyFieldKind(field)).drag_per_pixel;
}

std::string formatEditorPropertyValue(float value, EditorPropertyField field) {
  std::array<char, VALUE_TEXT_CAPACITY> text{};
  // Negative zero is the same number as zero and reads as a bug, so it is
  // written as zero.
  const float shown = value == 0.0f ? 0.0f : value;
  const int written = writeValue(text, shown, field);
  if (written <= 0) {
    return {};
  }
  // snprintf reports what it wanted to write, which for an absurd value is
  // more than the buffer holds; the string is whatever fitted.
  const size_t length = std::min(static_cast<size_t>(written), text.size() - 1);
  return {text.data(), length};
}

}  // namespace eng::editor
