#pragma once

/// @file editor-property-ops.h
/// @brief Read, write, step, and format a placement's editable properties.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-property-field.h>
#include <string>

namespace eng::editor {

/// How far one step button moves a distance, in tiles.
///
/// A quarter tile: small enough to nudge a prop off a grid line, large
/// enough that reaching the next tile is four clicks rather than a hundred.
inline constexpr float EDITOR_POSITION_STEP = 0.25f;

/// How far one step button turns an angle, in degrees.
inline constexpr float EDITOR_ROTATION_STEP = 15.0f;

/// Tiles a distance moves per pixel dragged.
///
/// One tile per tile-width of travel at zoom 1, so a drag across the value
/// box moves about as far on screen as the pointer did.
inline constexpr float EDITOR_POSITION_DRAG_PER_PIXEL = 1.0f / 64.0f;

/// Degrees an angle turns per pixel dragged.
inline constexpr float EDITOR_ROTATION_DRAG_PER_PIXEL = 1.0f;

/// Current value of one property.
[[nodiscard]] float editorPropertyValue(const EditorPlacement& placement,
                                        EditorPropertyField field);

/// Write one property. Angles are wrapped into [-180, 180), so turning past
/// half a revolution reads as a turn the other way rather than climbing
/// without bound.
void setEditorPropertyValue(EditorPlacement& placement,
                            EditorPropertyField field, float value);

/// How far one step button moves this property.
[[nodiscard]] float editorPropertyStep(EditorPropertyField field);

/// How far one dragged pixel moves this property.
[[nodiscard]] float editorPropertyDragPerPixel(EditorPropertyField field);

/// The value as the panel writes it: two decimals for a distance, one for
/// an angle. Fixed-point rather than the shortest round-trip, so a column
/// of values lines up and a number does not change width as it is dragged.
[[nodiscard]] std::string formatEditorPropertyValue(float value,
                                                    EditorPropertyField field);

}  // namespace eng::editor
