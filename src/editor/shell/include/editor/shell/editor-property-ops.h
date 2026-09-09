#pragma once

/// @file editor-property-ops.h
/// @brief Read, write, step, clamp, and format an editable property.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-property-traits.h>
#include <string>

namespace eng::editor {

/// How far one step button moves a distance, in tiles.
///
/// A quarter tile: small enough to nudge a prop off a grid line, large
/// enough that reaching the next tile is four clicks rather than a hundred.
inline constexpr float EDITOR_POSITION_STEP = 0.25f;

/// How far one step button turns an angle, in degrees.
inline constexpr float EDITOR_ROTATION_STEP = 15.0f;

/// How far one step button tips a direction component.
inline constexpr float EDITOR_AXIS_STEP = 0.05f;

/// How far one step button moves a multiplier.
inline constexpr float EDITOR_FACTOR_STEP = 0.1f;

/// How far one step button moves a fraction, a colour channel among them.
inline constexpr float EDITOR_UNIT_STEP = 0.05f;

/// Tiles a distance moves per pixel dragged.
///
/// One tile per tile-width of travel at zoom 1, so a drag across the value
/// box moves about as far on screen as the pointer did.
inline constexpr float EDITOR_POSITION_DRAG_PER_PIXEL = 1.0f / 64.0f;

/// Degrees an angle turns per pixel dragged.
inline constexpr float EDITOR_ROTATION_DRAG_PER_PIXEL = 1.0f;

/// How far a direction component tips per pixel dragged: the whole of its
/// range across a drag rather longer than the box, which is what a value
/// bounded either side wants.
inline constexpr float EDITOR_AXIS_DRAG_PER_PIXEL = 1.0f / 128.0f;

/// How far a multiplier moves per pixel dragged.
inline constexpr float EDITOR_FACTOR_DRAG_PER_PIXEL = 1.0f / 64.0f;

/// How far a fraction moves per pixel dragged.
inline constexpr float EDITOR_UNIT_DRAG_PER_PIXEL = 1.0f / 256.0f;

/// Current value of one of a placement's properties. A field a placement
/// does not have — a light's intensity — reads as zero.
[[nodiscard]] float editorPropertyValue(const EditorPlacement& placement,
                                        EditorPropertyField field);

/// Write one of a placement's properties, normalised as `normalizeEditor
/// PropertyValue` defines. A field a placement does not have is ignored.
void setEditorPropertyValue(EditorPlacement& placement,
                            EditorPropertyField field, float value);

/// The value a field will actually hold once given @p value: angles wrapped
/// into [-180, 180), fractions and direction components clamped to their
/// range, multipliers held at or above zero.
///
/// One place decides this, and both the document and the panel showing it
/// go through here, so the number on screen is never one the document would
/// have refused.
[[nodiscard]] float normalizeEditorPropertyValue(EditorPropertyField field,
                                                 float value);

/// How far one step button moves this property.
[[nodiscard]] float editorPropertyStep(EditorPropertyField field);

/// How far one dragged pixel moves this property.
[[nodiscard]] float editorPropertyDragPerPixel(EditorPropertyField field);

/// The value as the panel writes it: one decimal for an angle, two for
/// everything else. Fixed-point rather than the shortest round-trip, so a
/// column of values lines up and a number does not change width as it is
/// dragged.
[[nodiscard]] std::string formatEditorPropertyValue(float value,
                                                    EditorPropertyField field);

}  // namespace eng::editor
