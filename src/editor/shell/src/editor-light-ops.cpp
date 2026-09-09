#include "editor-vector-field.h"

#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <optional>

namespace eng::editor {

namespace {

  /// Whether a field names one of a light's position components.
  bool isPositionField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::POSITION_X);
  }

  /// Whether a field names one of a light's direction components.
  bool isDirectionField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::DIRECTION_X);
  }

  /// Whether a field names one of a light's colour channels.
  bool isColorField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::COLOR_R);
  }

  /// The component of one of a light's vectors that @p field names, or
  /// nothing when it names a scalar, or a field a light does not have.
  std::optional<float> vectorValue(const EditorLight& light,
                                   EditorPropertyField field) {
    if (isPositionField(field)) {
      return editorVectorValue(
          light.position,
          editorFieldAxis(field, EditorPropertyField::POSITION_X));
    }
    if (isDirectionField(field)) {
      return editorVectorValue(
          light.direction,
          editorFieldAxis(field, EditorPropertyField::DIRECTION_X));
    }
    if (isColorField(field)) {
      return editorVectorValue(
          light.color, editorFieldAxis(field, EditorPropertyField::COLOR_R));
    }
    return std::nullopt;
  }

  /// Write @p value into the vector component @p field names. False when it
  /// names none of them, which leaves the scalars to the caller.
  bool setVectorValue(EditorLight& light, EditorPropertyField field,
                      float value) {
    std::optional<size_t> axis;
    if (isPositionField(field)) {
      axis = editorFieldAxis(field, EditorPropertyField::POSITION_X);
      editorVectorAxis(light.position, *axis) = value;
    } else if (isDirectionField(field)) {
      axis = editorFieldAxis(field, EditorPropertyField::DIRECTION_X);
      editorVectorAxis(light.direction, *axis) = value;
    } else if (isColorField(field)) {
      axis = editorFieldAxis(field, EditorPropertyField::COLOR_R);
      editorVectorAxis(light.color, *axis) = value;
    }
    return axis.has_value();
  }

}  // namespace

std::string_view editorLightKindName(EditorLightKind kind) {
  return kind == EditorLightKind::DIRECTIONAL ? "Directional Light"
                                              : "Point Light";
}

EditorLight makeEditorLight(EditorLightKind kind, WorldPoint position) {
  EditorLight light;
  light.kind = kind;
  light.position = position;
  return light;
}

std::span<const EditorPropertyField> editorLightFields(EditorLightKind kind) {
  return kind == EditorLightKind::DIRECTIONAL
             ? std::span<
                   const EditorPropertyField>{EDITOR_DIRECTIONAL_LIGHT_FIELDS}
             : std::span<const EditorPropertyField>{EDITOR_POINT_LIGHT_FIELDS};
}

float editorLightValue(const EditorLight& light, EditorPropertyField field) {
  if (const std::optional<float> component = vectorValue(light, field)) {
    return *component;
  }
  if (field == EditorPropertyField::RANGE) {
    return light.range;
  }
  // Anything else is a field a light does not have — a placement's rotation,
  // say — and reads as zero rather than as some other property.
  return field == EditorPropertyField::INTENSITY ? light.intensity : 0.0f;
}

void setEditorLightValue(EditorLight& light, EditorPropertyField field,
                         float value) {
  const float written = normalizeEditorPropertyValue(field, value);
  if (setVectorValue(light, field, written)) {
    return;
  }
  if (field == EditorPropertyField::RANGE) {
    light.range = written;
  } else if (field == EditorPropertyField::INTENSITY) {
    light.intensity = written;
  }
}

MeshLight makeMeshLight(const EditorLight& light) {
  const bool point = light.kind == EditorLightKind::POINT;
  return {{light.position.x, light.position.y, light.position.z},
          point ? light.range : 0.0f,
          light.direction,
          light.intensity,
          light.color,
          point ? MESH_LIGHT_POINT : MESH_LIGHT_DIRECTIONAL};
}

}  // namespace eng::editor
