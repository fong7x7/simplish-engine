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

  /// Write one of a position's three components.
  void setPosition(WorldPoint& position, EditorPropertyField field,
                   float value) {
    if (field == EditorPropertyField::POSITION_X) {
      position.x = value;
    } else if (field == EditorPropertyField::POSITION_Y) {
      position.y = value;
    } else {
      position.z = value;
    }
  }

  /// Write one of a rotation's three components.
  void setRotation(Vec3& rotation, EditorPropertyField field, float value) {
    if (field == EditorPropertyField::ROTATION_X) {
      rotation.x = value;
    } else if (field == EditorPropertyField::ROTATION_Y) {
      rotation.y = value;
    } else {
      rotation.z = value;
    }
  }

}  // namespace

float editorPropertyValue(const EditorPlacement& placement,
                          EditorPropertyField field) {
  switch (field) {
    case EditorPropertyField::POSITION_X:
      return placement.position.x;
    case EditorPropertyField::POSITION_Y:
      return placement.position.y;
    case EditorPropertyField::POSITION_Z:
      return placement.position.z;
    case EditorPropertyField::ROTATION_X:
      return placement.rotation.x;
    case EditorPropertyField::ROTATION_Y:
      return placement.rotation.y;
    case EditorPropertyField::ROTATION_Z:
      return placement.rotation.z;
  }
  return 0.0f;
}

void setEditorPropertyValue(EditorPlacement& placement,
                            EditorPropertyField field, float value) {
  switch (field) {
    case EditorPropertyField::POSITION_X:
    case EditorPropertyField::POSITION_Y:
    case EditorPropertyField::POSITION_Z:
      setPosition(placement.position, field, value);
      return;
    case EditorPropertyField::ROTATION_X:
    case EditorPropertyField::ROTATION_Y:
    case EditorPropertyField::ROTATION_Z:
      setRotation(placement.rotation, field, wrapDegrees(value));
      return;
  }
}

float editorPropertyStep(EditorPropertyField field) {
  return editorPropertyFieldIsAngle(field) ? EDITOR_ROTATION_STEP
                                           : EDITOR_POSITION_STEP;
}

float editorPropertyDragPerPixel(EditorPropertyField field) {
  return editorPropertyFieldIsAngle(field) ? EDITOR_ROTATION_DRAG_PER_PIXEL
                                           : EDITOR_POSITION_DRAG_PER_PIXEL;
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
