#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-properties-widget.h>
#include <editor/shell/editor-property-ops.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-theme-constants.h>
#include <utility>

namespace eng::editor {

namespace {

  constexpr GuiColor VALUE_BG{30, 30, 34, 255};
  constexpr GuiColor STEP_BG{55, 55, 60, 255};
  /// Text baseline inset within a row, from its top.
  constexpr float TEXT_BASELINE = 4.0f;
  constexpr float TEXT_INSET = 6.0f;

  /// Vertically centred draw position for a line of text in @p rect.
  DrawPos textPos(const Rect& rect, float inset_x) {
    return drawPosInset(rect, inset_x, TEXT_BASELINE);
  }

}  // namespace

EditorPropertiesWidget::EditorPropertiesWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-properties";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  visible = false;
}

std::unique_ptr<GuiWidget> EditorPropertiesWidget::clone() const {
  return std::make_unique<EditorPropertiesWidget>(*this);
}

float EditorPropertiesWidget::preferredWidth() const {
  return has_selection_ ? PROPERTIES_PANEL_WIDTH : 0.0f;
}

EditorPropertiesLayout EditorPropertiesWidget::layout() const {
  return layoutEditorProperties(rect);
}

size_t EditorPropertiesWidget::rowOf(EditorPropertyField field) const {
  size_t row = 0;
  while (row < fields_.size() && fields_[row] != field) {
    ++row;
  }
  return row;
}

float EditorPropertiesWidget::value(EditorPropertyField field) const {
  const size_t row = rowOf(field);
  return row < values_.size() ? values_[row] : 0.0f;
}

Rect EditorPropertiesWidget::fieldRowRect(EditorPropertyField field) const {
  const size_t row = rowOf(field);
  // A field this selection does not list has no row to point at, and an
  // out-of-range one would be another field's.
  if (row >= fields_.size()) {
    return {};
  }
  return propertyRowRect(layout().body, row);
}

void EditorPropertiesWidget::beginSelection(
    std::string name, std::span<const EditorPropertyField> fields) {
  name_ = std::move(name);
  fields_.assign(fields.begin(), fields.end());
  values_.assign(fields_.size(), 0.0f);
  has_selection_ = true;
  visible = true;
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorPlacement& placement) {
  beginSelection(std::move(name), EDITOR_PLACEMENT_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorPropertyValue(placement, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorLight& light) {
  beginSelection(std::move(name), editorLightFields(light.kind));
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorLightValue(light, fields_[row]);
  }
}

void EditorPropertiesWidget::clearSelection() {
  has_selection_ = false;
  visible = false;
  // A drag whose subject has gone has nothing left to commit.
  dragging_ = false;
  name_.clear();
  fields_.clear();
  values_.clear();
}

void EditorPropertiesWidget::renderHeader(const GuiDrawContext& ctx) const {
  const Rect header = layout().header;
  ctx.drawFilledRect(header, GuiColor::applyOpacity(THEME_BG, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(header, TEXT_INSET), "Properties");
}

void EditorPropertiesWidget::renderNameLine(const GuiDrawContext& ctx) const {
  const Rect line = layout().asset;
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               textPos(line, TEXT_INSET), name_);
}

void EditorPropertiesWidget::renderStep(const GuiDrawContext& ctx,
                                        const Rect& box,
                                        std::string_view sign) const {
  ctx.drawRoundedRect(box, GuiColor::applyOpacity(STEP_BG, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(box, GuiColor::applyOpacity(THEME_TEXT, opacity), sign);
}

void EditorPropertiesWidget::renderRow(const GuiDrawContext& ctx,
                                       size_t index) const {
  const EditorPropertyField field = fields_[index];
  const Rect row = propertyRowRect(layout().body, index);
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(propertyLabelRect(row), 0.0f),
               editorPropertyFieldLabel(field));
  renderStep(ctx, propertyDecrementRect(row), "-");
  renderStep(ctx, propertyIncrementRect(row), "+");

  const Rect value = propertyValueRect(row);
  const bool active = dragging_ && drag_field_ == field;
  ctx.drawRoundedRect(
      value, GuiColor::applyOpacity(active ? THEME_ACCENT : VALUE_BG, opacity),
      THEME_BTN_RADIUS);
  ctx.drawCenteredText(value, GuiColor::applyOpacity(THEME_TEXT, opacity),
                       formatEditorPropertyValue(values_[index], field));
}

void EditorPropertiesWidget::renderRows(const GuiDrawContext& ctx) const {
  for (size_t row = 0; row < fields_.size(); ++row) {
    renderRow(ctx, row);
  }
}

void EditorPropertiesWidget::render(const GuiDrawContext& ctx) const {
  if (!has_selection_ || rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  renderPanel({ctx, GuiColor::applyOpacity(fill_color, opacity)});
  renderHeader(ctx);
  renderNameLine(ctx);
  renderRows(ctx);
}

void EditorPropertiesWidget::applyValue(EditorPropertyField field, float value,
                                        EditorPropertyEdit edit) {
  const size_t row = rowOf(field);
  if (row >= values_.size()) {
    return;
  }
  // Normalised here, through the same rule the document applies, so what
  // the panel draws next frame is what the document will hold — a wrapped
  // angle and a clamped colour included.
  values_[row] = normalizeEditorPropertyValue(field, value);
  if (on_property_changed) {
    on_property_changed(field, values_[row], edit);
  }
}

void EditorPropertiesWidget::stepField(EditorPropertyField field, float steps) {
  applyValue(field, value(field) + steps * editorPropertyStep(field),
             EditorPropertyEdit::COMMIT);
}

bool EditorPropertiesWidget::pressStep(EditorPropertyField field,
                                       const Rect& row,
                                       const GuiMouseEvent& event) {
  if (containsPoint(propertyDecrementRect(row), event.x, event.y)) {
    stepField(field, -1.0f);
    return true;
  }
  if (containsPoint(propertyIncrementRect(row), event.x, event.y)) {
    stepField(field, 1.0f);
    return true;
  }
  return false;
}

void EditorPropertiesWidget::beginDrag(EditorPropertyField field,
                                       const GuiMouseEvent& event) {
  dragging_ = true;
  drag_field_ = field;
  drag_start_value_ = value(field);
  drag_start_x_ = event.x;
}

bool EditorPropertiesWidget::pressRow(size_t index,
                                      const GuiMouseEvent& event) {
  const EditorPropertyField field = fields_[index];
  const Rect row = propertyRowRect(layout().body, index);
  // A step is done the moment it is pressed, so it never takes capture.
  if (pressStep(field, row, event) ||
      !containsPoint(propertyValueRect(row), event.x, event.y)) {
    return false;
  }
  beginDrag(field, event);
  return true;
}

bool EditorPropertiesWidget::handleMouseDown(const GuiMouseEvent& event) {
  if (!has_selection_ || event.button != GuiMouseButton::LEFT) {
    return false;
  }
  const int row =
      hitTestPropertyRow(layout().body, fields_.size(), event.x, event.y);
  if (row < 0) {
    return false;
  }
  return pressRow(static_cast<size_t>(row), event);
}

void EditorPropertiesWidget::handleMouseMove(const GuiMouseEvent& event) {
  if (!dragging_) {
    return;
  }
  applyValue(drag_field_,
             drag_start_value_ + (event.x - drag_start_x_) *
                                     editorPropertyDragPerPixel(drag_field_),
             EditorPropertyEdit::PREVIEW);
}

void EditorPropertiesWidget::handleMouseUp(const GuiMouseEvent& /*event*/) {
  if (!dragging_) {
    return;
  }
  dragging_ = false;
  // The value is already where the last move put it; this says the gesture
  // is over, which is what turns the run of previews into one undo entry.
  applyValue(drag_field_, value(drag_field_), EditorPropertyEdit::COMMIT);
}

}  // namespace eng::editor
