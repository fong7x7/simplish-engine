#pragma once

/// @file editor-property-field.h
/// @brief The placement properties the panel lists, one row each.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace eng::editor {

/// One editable number on a placement.
///
/// The panel is a list of these rather than a hand-written form: the rows,
/// the hit testing, and the edits all run off this enum, so a property the
/// level format grows later is a new entry here and a case in
/// `editor-property-ops.cpp`, not another block of layout code.
/// @thread_safety Immutable value type.
enum class EditorPropertyField : uint8_t {
  /// World X of the placement, in tiles.
  POSITION_X,
  /// World Y of the placement, in tiles.
  POSITION_Y,
  /// Height above the ground plane, in tiles.
  POSITION_Z,
  /// Rotation about world X, in degrees.
  ROTATION_X,
  /// Rotation about world Y, in degrees.
  ROTATION_Y,
  /// Rotation about world Z, in degrees.
  ROTATION_Z,
};

/// Every property, in the order the panel lists them.
inline constexpr EditorPropertyField EDITOR_PROPERTY_FIELDS[] = {
    EditorPropertyField::POSITION_X, EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z, EditorPropertyField::ROTATION_X,
    EditorPropertyField::ROTATION_Y, EditorPropertyField::ROTATION_Z,
};

/// How many rows the panel has.
inline constexpr size_t EDITOR_PROPERTY_FIELD_COUNT =
    sizeof(EDITOR_PROPERTY_FIELDS) / sizeof(EDITOR_PROPERTY_FIELDS[0]);

/// Row label. Spelled out per row rather than grouped under two headings:
/// six unambiguous labels read the same and cost the panel no rows a
/// pointer cannot hit.
[[nodiscard]] constexpr std::string_view
editorPropertyFieldLabel(EditorPropertyField field) {
  switch (field) {
    case EditorPropertyField::POSITION_X:
      return "Position X";
    case EditorPropertyField::POSITION_Y:
      return "Position Y";
    case EditorPropertyField::POSITION_Z:
      return "Position Z";
    case EditorPropertyField::ROTATION_X:
      return "Rotation X";
    case EditorPropertyField::ROTATION_Y:
      return "Rotation Y";
    case EditorPropertyField::ROTATION_Z:
      return "Rotation Z";
  }
  return {};
}

/// Whether a field is an angle, which decides how it steps, how it is
/// written out, and whether it wraps.
[[nodiscard]] constexpr bool
editorPropertyFieldIsAngle(EditorPropertyField field) {
  switch (field) {
    case EditorPropertyField::POSITION_X:
    case EditorPropertyField::POSITION_Y:
    case EditorPropertyField::POSITION_Z:
      return false;
    case EditorPropertyField::ROTATION_X:
    case EditorPropertyField::ROTATION_Y:
    case EditorPropertyField::ROTATION_Z:
      return true;
  }
  return false;
}

}  // namespace eng::editor
