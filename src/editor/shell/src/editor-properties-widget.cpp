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

Rect EditorPropertiesWidget::fieldRowRect(EditorPropertyField field) const {
  return propertyRowRect(layout().body, static_cast<size_t>(field));
}

void EditorPropertiesWidget::setSelection(std::string asset_name,
                                          const EditorPlacement& placement) {
  asset_name_ = std::move(asset_name);
  placement_ = placement;
  has_selection_ = true;
  visible = true;
}

void EditorPropertiesWidget::clearSelection() {
  has_selection_ = false;
  visible = false;
  // A drag whose placement has gone has nothing left to commit.
  dragging_ = false;
  asset_name_.clear();
  placement_ = EditorPlacement{};
}

void EditorPropertiesWidget::renderHeader(const GuiDrawContext& ctx) const {
  const Rect header = layout().header;
  ctx.drawFilledRect(header, GuiColor::applyOpacity(THEME_BG, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(header, TEXT_INSET), "Properties");
}

void EditorPropertiesWidget::renderAssetLine(const GuiDrawContext& ctx) const {
  const Rect line = layout().asset;
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               textPos(line, TEXT_INSET), asset_name_);
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
  const EditorPropertyField field = EDITOR_PROPERTY_FIELDS[index];
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
  ctx.drawCenteredText(
      value, GuiColor::applyOpacity(THEME_TEXT, opacity),
      formatEditorPropertyValue(editorPropertyValue(placement_, field), field));
}

void EditorPropertiesWidget::renderRows(const GuiDrawContext& ctx) const {
  for (size_t i = 0; i < EDITOR_PROPERTY_FIELD_COUNT; ++i) {
    renderRow(ctx, i);
  }
}

void EditorPropertiesWidget::render(const GuiDrawContext& ctx) const {
  if (!has_selection_ || rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  renderPanel({ctx, GuiColor::applyOpacity(fill_color, opacity)});
  renderHeader(ctx);
  renderAssetLine(ctx);
  renderRows(ctx);
}

void EditorPropertiesWidget::applyValue(EditorPropertyField field, float value,
                                        EditorPropertyEdit edit) {
  // The panel writes the value into its own copy through the same setter
  // the editor uses, so what it draws next frame is what the placement will
  // hold — a wrapped angle included.
  setEditorPropertyValue(placement_, field, value);
  if (on_property_changed) {
    on_property_changed(field, editorPropertyValue(placement_, field), edit);
  }
}

void EditorPropertiesWidget::stepField(EditorPropertyField field, float steps) {
  applyValue(field,
             editorPropertyValue(placement_, field) +
                 steps * editorPropertyStep(field),
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
  drag_start_value_ = editorPropertyValue(placement_, field);
  drag_start_x_ = event.x;
}

bool EditorPropertiesWidget::pressRow(size_t index,
                                      const GuiMouseEvent& event) {
  const EditorPropertyField field = EDITOR_PROPERTY_FIELDS[index];
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
  const int row = hitTestPropertyRow(layout().body, event.x, event.y);
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
  applyValue(drag_field_, editorPropertyValue(placement_, drag_field_),
             EditorPropertyEdit::COMMIT);
}

}  // namespace eng::editor
