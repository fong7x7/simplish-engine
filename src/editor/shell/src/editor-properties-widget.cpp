#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-properties-widget.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-scale-slider.h>
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
  /// How far a checkbox's tick sits inside its box.
  constexpr float CHECK_INSET = 5.0f;
  /// The part of a slider's track already travelled, behind the readout.
  constexpr GuiColor SLIDER_FILL{0, 122, 204, 110};
  /// The mark at 1 on a scale slider, the size an asset was dropped at.
  constexpr GuiColor SLIDER_MARK{120, 120, 120, 255};
  /// Width of the slider's handle and of the mark at 1.
  constexpr float SLIDER_LINE = 2.0f;

  /// A full-height line across a slider's @p track at @p x: its handle, or
  /// the mark at 1.
  Rect sliderLineRect(const Rect& track, float x) {
    return makeRect(x - SLIDER_LINE * 0.5f, track.y, SLIDER_LINE, track.h);
  }

  /// The part of @p track from its left end to the handle at @p knob.
  Rect sliderTravelledRect(const Rect& track, float knob) {
    return makeRect(track.x, track.y, knob - track.x, track.h);
  }

  /// Vertically centred draw position for a line of text in @p rect.
  DrawPos textPos(const Rect& rect, float inset_x) {
    return drawPosInset(rect, inset_x, TEXT_BASELINE);
  }

  /// The choice @p steps along from @p current among @p count, wrapping
  /// round at either end whichever way it stepped and however far.
  size_t stepChoice(size_t count, size_t current, int steps) {
    const auto n = static_cast<long>(count);
    const long to = ((static_cast<long>(current) + steps) % n + n) % n;
    return static_cast<size_t>(to);
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
    std::string name, std::string reference,
    std::span<const EditorPropertyField> fields) {
  name_ = std::move(name);
  reference_ = std::move(reference);
  fields_.assign(fields.begin(), fields.end());
  values_.assign(fields_.size(), 0.0f);
  choice_label_.clear();
  choices_.clear();
  choice_ = 0;
  has_selection_ = true;
  visible = true;
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorPlacement& placement) {
  beginSelection(std::move(name), editorPlacementRef(placement),
                 EDITOR_PLACEMENT_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorPropertyValue(placement, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorLight& light) {
  beginSelection(std::move(name), editorLightRef(light),
                 editorLightFields(light.kind));
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorLightValue(light, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorPlayerStart& start) {
  beginSelection(std::move(name), editorPlayerStartRef(start),
                 EDITOR_PLAYER_START_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorPlayerStartValue(start, fields_[row]);
  }
}

void EditorPropertiesWidget::setChoices(std::string_view label,
                                        std::vector<std::string> choices,
                                        size_t current) {
  choice_label_ = label;
  choices_ = std::move(choices);
  choice_ = current < choices_.size() ? current : 0;
}

const std::string& EditorPropertiesWidget::choice() const {
  static const std::string none;
  return choices_.empty() ? none : choices_[choice_];
}

size_t EditorPropertiesWidget::rowCount() const {
  return fields_.size() + (choices_.empty() ? 0 : 1);
}

Rect EditorPropertiesWidget::choiceRowRect() const {
  return choices_.empty() ? Rect{}
                          : propertyRowRect(layout().body, fields_.size());
}

void EditorPropertiesWidget::clearSelection() {
  choice_label_.clear();
  choices_.clear();
  choice_ = 0;
  has_selection_ = false;
  visible = false;
  // A drag whose subject has gone has nothing left to commit.
  dragging_ = false;
  name_.clear();
  reference_.clear();
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

void EditorPropertiesWidget::renderIdLine(const GuiDrawContext& ctx) const {
  const Rect line = layout().id;
  // Dimmer than the name above it, and the panel's one piece of text that
  // is not prose: this is a reference to be copied verbatim.
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               textPos(line, TEXT_INSET), reference_);
}

void EditorPropertiesWidget::renderStep(const GuiDrawContext& ctx,
                                        const Rect& box,
                                        std::string_view sign) const {
  ctx.drawRoundedRect(box, GuiColor::applyOpacity(STEP_BG, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(box, GuiColor::applyOpacity(THEME_TEXT, opacity), sign);
}

void EditorPropertiesWidget::renderToggle(const GuiDrawContext& ctx,
                                          const Rect& row, size_t index) const {
  const Rect box = propertyCheckboxRect(row);
  ctx.drawRoundedRect(box, GuiColor::applyOpacity(VALUE_BG, opacity),
                      THEME_BTN_RADIUS);
  if (values_[index] == 0.0f) {
    return;
  }
  // Ticked: the box filled in the accent colour, inset so the unticked
  // well still frames it.
  const Rect tick =
      makeRect(box.x + CHECK_INSET, box.y + CHECK_INSET,
               box.w - CHECK_INSET * 2.0f, box.h - CHECK_INSET * 2.0f);
  ctx.drawRoundedRect(tick, GuiColor::applyOpacity(THEME_ACCENT, opacity),
                      THEME_BTN_RADIUS);
}

void EditorPropertiesWidget::renderRow(const GuiDrawContext& ctx,
                                       size_t index) const {
  const EditorPropertyField field = fields_[index];
  const Rect row = propertyRowRect(layout().body, index);
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(propertyLabelRect(row), 0.0f),
               editorPropertyFieldLabel(field));
  if (editorPropertyFieldIsToggle(field)) {
    renderToggle(ctx, row, index);
    return;
  }
  renderStep(ctx, propertyDecrementRect(row), "-");
  renderStep(ctx, propertyIncrementRect(row), "+");
  if (editorPropertyFieldIsScale(field)) {
    renderSlider(ctx, propertyValueRect(row), index);
    return;
  }
  renderValueBox(ctx, row, index);
}

void EditorPropertiesWidget::renderSlider(const GuiDrawContext& ctx,
                                          const Rect& track,
                                          size_t index) const {
  const float knob =
      track.x + track.w * editorScaleSliderFraction(values_[index]);
  ctx.drawRoundedRect(track, GuiColor::applyOpacity(VALUE_BG, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawRoundedRect(sliderTravelledRect(track, knob),
                      GuiColor::applyOpacity(SLIDER_FILL, opacity),
                      THEME_BTN_RADIUS);
  // The middle of the track is 1 exactly, so a mark there says where the
  // size a prop was dropped at is, whichever way it has been pushed.
  ctx.drawFilledRect(sliderLineRect(track, track.x + track.w * 0.5f),
                     GuiColor::applyOpacity(SLIDER_MARK, opacity));
  ctx.drawFilledRect(sliderLineRect(track, knob),
                     GuiColor::applyOpacity(THEME_TEXT, opacity));
  ctx.drawCenteredText(
      track, GuiColor::applyOpacity(THEME_TEXT, opacity),
      formatEditorPropertyValue(values_[index], fields_[index]));
}

void EditorPropertiesWidget::renderValueBox(const GuiDrawContext& ctx,
                                            const Rect& row,
                                            size_t index) const {
  const EditorPropertyField field = fields_[index];
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
  renderChoiceRow(ctx);
}

void EditorPropertiesWidget::renderChoiceRow(const GuiDrawContext& ctx) const {
  if (choices_.empty()) {
    return;
  }
  const Rect row = choiceRowRect();
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(propertyLabelRect(row), 0.0f), choice_label_);
  renderStep(ctx, propertyDecrementRect(row), "-");
  renderStep(ctx, propertyIncrementRect(row), "+");
  const Rect value = propertyValueRect(row);
  ctx.drawRoundedRect(value, GuiColor::applyOpacity(VALUE_BG, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(value, GuiColor::applyOpacity(THEME_TEXT, opacity),
                       choice());
}

void EditorPropertiesWidget::render(const GuiDrawContext& ctx) const {
  if (!has_selection_ || rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  renderPanel({ctx, GuiColor::applyOpacity(fill_color, opacity)});
  renderHeader(ctx);
  renderNameLine(ctx);
  renderIdLine(ctx);
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
  // A scale steps between fixed stops, not by an amount: see
  // `editorScaleStepped` for why that is what lands it back on 1.
  const float stepped =
      editorPropertyFieldIsScale(field)
          ? editorScaleStepped(value(field), static_cast<int>(steps))
          : value(field) + steps * editorPropertyStep(field);
  applyValue(field, stepped, EditorPropertyEdit::COMMIT);
}

float EditorPropertiesWidget::sliderValueAt(EditorPropertyField field,
                                            float x) const {
  const Rect track = propertyValueRect(fieldRowRect(field));
  const float fraction = track.w > 0.0f ? (x - track.x) / track.w : 0.5f;
  return editorScaleFromSliderFraction(fraction);
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
  if (editorPropertyFieldIsScale(field)) {
    // Straight to where it was pressed, as a slider does, rather than
    // waiting for the pointer to move before anything happens.
    applyValue(field, sliderValueAt(field, event.x),
               EditorPropertyEdit::PREVIEW);
  }
}

bool EditorPropertiesWidget::pressRow(size_t index,
                                      const GuiMouseEvent& event) {
  const EditorPropertyField field = fields_[index];
  // A toggle flips on a press anywhere in its row, label included, and is
  // done at once: there is no gesture to capture.
  if (editorPropertyFieldIsToggle(field)) {
    applyValue(field, values_[index] == 0.0f ? 1.0f : 0.0f,
               EditorPropertyEdit::COMMIT);
    return false;
  }
  const Rect row = propertyRowRect(layout().body, index);
  // A step is done the moment it is pressed, so it never takes capture.
  if (pressStep(field, row, event) ||
      !containsPoint(propertyValueRect(row), event.x, event.y)) {
    return false;
  }
  beginDrag(field, event);
  return true;
}

void EditorPropertiesWidget::pressChoiceRow(const GuiMouseEvent& event) {
  const Rect row = choiceRowRect();
  int steps = 0;
  if (containsPoint(propertyDecrementRect(row), event.x, event.y)) {
    steps = -1;
  } else if (containsPoint(propertyIncrementRect(row), event.x, event.y)) {
    steps = 1;
  }
  const size_t next = stepChoice(choices_.size(), choice_, steps);
  if (steps == 0 || next == choice_) {
    return;
  }
  choice_ = next;
  if (on_choice_changed) {
    on_choice_changed(choice_);
  }
}

bool EditorPropertiesWidget::handleMouseDown(const GuiMouseEvent& event) {
  if (!has_selection_ || event.button != GuiMouseButton::LEFT) {
    return false;
  }
  const int row =
      hitTestPropertyRow(layout().body, rowCount(), event.x, event.y);
  if (row < 0) {
    return false;
  }
  if (static_cast<size_t>(row) == fields_.size()) {
    // The choice row: a step is done the moment it is pressed, so it never
    // takes capture.
    pressChoiceRow(event);
    return false;
  }
  return pressRow(static_cast<size_t>(row), event);
}

void EditorPropertiesWidget::handleMouseMove(const GuiMouseEvent& event) {
  if (!dragging_) {
    return;
  }
  // A slider's value is wherever the pointer is along it; a value box's is
  // how far the pointer has travelled since the press.
  const float next =
      editorPropertyFieldIsScale(drag_field_)
          ? sliderValueAt(drag_field_, event.x)
          : drag_start_value_ + (event.x - drag_start_x_) *
                                    editorPropertyDragPerPixel(drag_field_);
  applyValue(drag_field_, next, EditorPropertyEdit::PREVIEW);
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
