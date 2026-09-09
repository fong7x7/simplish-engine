#include <algorithm>
#include <editor/shell/editor-properties-layout.h>

namespace eng::editor {

namespace {

  /// Take @p height off the top of @p remaining, or as much as is left.
  Rect takeTop(Rect& remaining, float height) {
    const float taken = std::min(std::max(height, 0.0f), remaining.h);
    const Rect strip = makeRect(remaining.x, remaining.y, remaining.w, taken);
    remaining.y += taken;
    remaining.h -= taken;
    return strip;
  }

  /// Height one row occupies, its gap included.
  float rowPitch() {
    return PROPERTIES_ROW_HEIGHT + PROPERTIES_ROW_GAP;
  }

}  // namespace

EditorPropertiesLayout layoutEditorProperties(const Rect& panel) {
  EditorPropertiesLayout out{};
  Rect remaining = panel;
  out.header = takeTop(remaining, PROPERTIES_HEADER_HEIGHT);
  out.asset = takeTop(remaining, PROPERTIES_ASSET_HEIGHT);
  out.id = takeTop(remaining, PROPERTIES_ID_HEIGHT);
  // The body is inset on three sides; the header and the two lines under
  // it span the panel so their backgrounds meet its edges.
  const float inset = std::min(PROPERTIES_PADDING, remaining.w * 0.5f);
  out.body = makeRect(remaining.x + inset, remaining.y + PROPERTIES_ROW_GAP,
                      std::max(0.0f, remaining.w - inset * 2.0f),
                      std::max(0.0f, remaining.h - PROPERTIES_ROW_GAP));
  return out;
}

Rect propertyRowRect(const Rect& body, size_t index) {
  return makeRect(body.x, body.y + static_cast<float>(index) * rowPitch(),
                  body.w, PROPERTIES_ROW_HEIGHT);
}

Rect propertyLabelRect(const Rect& row) {
  return makeRect(row.x, row.y, std::min(PROPERTIES_LABEL_WIDTH, row.w), row.h);
}

Rect propertyDecrementRect(const Rect& row) {
  const Rect label = propertyLabelRect(row);
  const float x = label.x + label.w;
  return makeRect(x, row.y, std::min(PROPERTIES_STEP_WIDTH, row.w - label.w),
                  row.h);
}

Rect propertyIncrementRect(const Rect& row) {
  const float width = std::min(PROPERTIES_STEP_WIDTH, row.w);
  return makeRect(row.x + row.w - width, row.y, width, row.h);
}

Rect propertyValueRect(const Rect& row) {
  const Rect decrement = propertyDecrementRect(row);
  const Rect increment = propertyIncrementRect(row);
  const float x = decrement.x + decrement.w;
  return makeRect(x, row.y, std::max(0.0f, increment.x - x), row.h);
}

int hitTestPropertyRow(const Rect& body, size_t rows, float x, float y) {
  for (size_t i = 0; i < rows; ++i) {
    if (containsPoint(propertyRowRect(body, i), x, y)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace eng::editor
