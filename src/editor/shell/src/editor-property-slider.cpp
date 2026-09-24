#include <algorithm>
#include <editor/shell/editor-property-slider.h>
#include <editor/shell/editor-property-traits.h>
#include <editor/shell/editor-scale-slider.h>

namespace eng::editor {

float editorPropertySliderFraction(EditorPropertyField field, float value) {
  return editorPropertyFieldIsScale(field) ? editorScaleSliderFraction(value)
                                           : std::clamp(value, 0.0f, 1.0f);
}

float editorPropertyFromSliderFraction(EditorPropertyField field,
                                       float fraction) {
  return editorPropertyFieldIsScale(field)
             ? editorScaleFromSliderFraction(fraction)
             : std::clamp(fraction, 0.0f, 1.0f);
}

}  // namespace eng::editor
