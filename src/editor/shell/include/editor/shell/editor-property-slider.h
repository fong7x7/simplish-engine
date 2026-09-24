#pragma once

/// @file editor-property-slider.h
/// @brief Where a slider row's value sits along its track, and back.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-property-field.h>

namespace eng::editor {

/// Where @p value of @p field sits along its slider, 0 at the left end to 1
/// at the right: a scale's logarithmic place (`editor-scale-slider.h`), a
/// shade's own value.
[[nodiscard]] float editorPropertySliderFraction(EditorPropertyField field,
                                                 float value);

/// The value of @p field at @p fraction along its slider — the inverse of
/// `editorPropertySliderFraction`, held to the slider's ends.
[[nodiscard]] float editorPropertyFromSliderFraction(EditorPropertyField field,
                                                     float fraction);

}  // namespace eng::editor
