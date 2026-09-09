#include "editor-vector-field.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <editor/shell/editor-property-ops.h>

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

  /// Whether a field names one of a placement's position components.
  bool isPositionField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::POSITION_X);
  }

  /// Whether a field names one of a placement's rotation components.
  bool isRotationField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::ROTATION_X);
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
  return 0.0f;
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
  }
}

float editorPropertyStep(EditorPropertyField field) {
  switch (editorPropertyFieldKind(field)) {
    case EditorPropertyKind::DISTANCE:
    case EditorPropertyKind::EXTENT:
      return EDITOR_POSITION_STEP;
    case EditorPropertyKind::ANGLE:
      return EDITOR_ROTATION_STEP;
    case EditorPropertyKind::AXIS:
      return EDITOR_AXIS_STEP;
    case EditorPropertyKind::FACTOR:
      return EDITOR_FACTOR_STEP;
    case EditorPropertyKind::UNIT:
      return EDITOR_UNIT_STEP;
  }
  return EDITOR_POSITION_STEP;
}

float editorPropertyDragPerPixel(EditorPropertyField field) {
  switch (editorPropertyFieldKind(field)) {
    case EditorPropertyKind::DISTANCE:
    case EditorPropertyKind::EXTENT:
      return EDITOR_POSITION_DRAG_PER_PIXEL;
    case EditorPropertyKind::ANGLE:
      return EDITOR_ROTATION_DRAG_PER_PIXEL;
    case EditorPropertyKind::AXIS:
      return EDITOR_AXIS_DRAG_PER_PIXEL;
    case EditorPropertyKind::FACTOR:
      return EDITOR_FACTOR_DRAG_PER_PIXEL;
    case EditorPropertyKind::UNIT:
      return EDITOR_UNIT_DRAG_PER_PIXEL;
  }
  return EDITOR_POSITION_DRAG_PER_PIXEL;
}

std::string formatEditorPropertyValue(float value, EditorPropertyField field) {
  std::array<char, VALUE_TEXT_CAPACITY> text{};
  // Negative zero is the same number as zero and reads as a bug, so it is
  // written as zero.
  const float shown = value == 0.0f ? 0.0f : value;
  const int written =
      std::snprintf(text.data(), text.size(),
                    editorPropertyFieldIsAngle(field) ? "%.1f" : "%.2f", shown);
  if (written <= 0) {
    return {};
  }
  // snprintf reports what it wanted to write, which for an absurd value is
  // more than the buffer holds; the string is whatever fitted.
  const size_t length = std::min(static_cast<size_t>(written), text.size() - 1);
  return {text.data(), length};
}

}  // namespace eng::editor
